#include "UI/QuickSlotManager.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryItem.h"
#include "Inventory/ItemUseLibrary.h"
#include "UObject/UnrealType.h"

UQuickSlotManager::UQuickSlotManager()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UQuickSlotManager::BeginPlay()
{
    Super::BeginPlay();
    Slots.SetNum(NumSlots);
    for (int32 i = 0; i < NumSlots; ++i) Slots[i].SlotIndex = i;
}

bool UQuickSlotManager::AssignFromInventory(int32 QuickIndex, UInventoryComponent* SourceInv, int32 SourceIdx)
{
    if (!SourceInv || !SourceInv->IsValidIndex_Public(SourceIdx)) return false;
    if (QuickIndex < 0 || QuickIndex >= NumSlots) return false;

    UInventoryItem* Item = SourceInv->GetItemAt(SourceIdx);
    if (!Item) return false;

    // 여기서 ‘장비 등’ 허용 여부를 최종 판단 (소모/퀘스트/스킬만 허용)
    if (!IsAllowedForQuickslot(Item))
    {
        UE_LOG(LogTemp, Warning, TEXT("[QS] Reject assign: Quick=%d, Item=%s (blocked by filter)"),
            QuickIndex, *GetNameSafe(Item));
        return false;
    }
    // 동일 ItemId 중복 등록 제거
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (i == QuickIndex) continue;
        if (Slots[i].ItemId == Item->ItemId)
        {
            Slots[i].Inventory = nullptr;
            Slots[i].ItemInstanceId.Invalidate();
            Slots[i].ItemId = NAME_None;
            OnQuickSlotChanged.Broadcast(i, nullptr);
        }
    }

    FQuickSlotEntry& E = Slots[QuickIndex];
    E.Inventory = SourceInv;
    E.ItemInstanceId = Item->InstanceId;
    E.ItemId = Item->ItemId;        //  타입 기억

    EnsureInventoryObserved(SourceInv);     //  변경 감지 바인딩
    OnQuickSlotChanged.Broadcast(QuickIndex, Item);
    return true;
}



bool UQuickSlotManager::ClearSlot(int32 QuickIndex)
{
    if (QuickIndex < 0 || QuickIndex >= NumSlots) return false;
    FQuickSlotEntry& E = Slots[QuickIndex];
    E.Inventory = nullptr;
    E.ItemInstanceId.Invalidate();
    E.ItemId = NAME_None;              // ★ 초기화

    OnQuickSlotChanged.Broadcast(QuickIndex, nullptr);
    return true;
}

UInventoryItem* UQuickSlotManager::FindItemByInstance(UInventoryComponent* Inv, const FGuid& InstanceId) const
{
    if (!Inv || !InstanceId.IsValid()) return nullptr;
    const int32 Count = Inv->GetSlotCount();
    for (int32 i = 0; i < Count; ++i)
    {
        if (UInventoryItem* It = Inv->GetItemAt(i))
        {
            if (It->InstanceId == InstanceId) return It;
        }
    }
    return nullptr;
}

UInventoryItem* UQuickSlotManager::ResolveItem(int32 QuickIndex) const
{
    if (QuickIndex < 0 || QuickIndex >= NumSlots) return nullptr;
    const FQuickSlotEntry& E = Slots[QuickIndex];
    return FindItemByInstance(E.Inventory.Get(), E.ItemInstanceId);
}

// 총 수량 계산 (ItemId 기준, 없으면 ResolveItem로 보강)
int32 UQuickSlotManager::GetTotalCountForSlot(int32 QuickIndex) const
{
    if (QuickIndex < 0 || QuickIndex >= NumSlots) return 0;
    const FQuickSlotEntry& E = Slots[QuickIndex];
    if (E.ItemId.IsNone() || !E.Inventory.IsValid()) return 0;

    UInventoryComponent* Inv = E.Inventory.Get();
    int32 Total = 0;
    const int32 Count = Inv->GetSlotCount();
    for (int32 i = 0; i < Count; ++i)
    {
        if (UInventoryItem* It = Inv->GetItemAt(i))
        {
            if (It->ItemId == E.ItemId) Total += It->Quantity;
        }
    }
    return Total;
}


void UQuickSlotManager::EnsureInventoryObserved(UInventoryComponent* Inv)
{
    if (!Inv) return;
    if (Observed.Contains(Inv)) return;

    Observed.Add(Inv);
    Inv->OnSlotUpdated.AddDynamic(this, &UQuickSlotManager::OnInventorySlotUpdated);
    Inv->OnInventoryRefreshed.AddDynamic(this, &UQuickSlotManager::OnInventoryRefreshed);
}

void UQuickSlotManager::OnInventorySlotUpdated(int32 /*UpdatedIndex*/, UInventoryItem* /*UpdatedItem*/)
{
    // 관찰 중인 인벤토리를 참조하는 슬롯들을 갱신/재바인딩
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        FQuickSlotEntry& E = Slots[i];
        if (!E.Inventory.IsValid()) continue;

        // 원래 인스턴스가 스택 합치기 등으로 사라졌다면 같은 ItemId의 다른 인스턴스로 보정
        if (E.ItemId.IsNone()) { OnQuickSlotChanged.Broadcast(i, nullptr); continue; }

        if (!FindItemByInstance(E.Inventory.Get(), E.ItemInstanceId))
        {
            const int32 Count = E.Inventory->GetSlotCount();
            for (int32 s = 0; s < Count; ++s)
            {
                if (UInventoryItem* It = E.Inventory->GetItemAt(s))
                {
                    if (It->ItemId == E.ItemId) { E.ItemInstanceId = It->InstanceId; break; }
                }
            }
        }

        OnQuickSlotChanged.Broadcast(i, ResolveItem(i)); // 위젯이 총수량/아이콘 갱신
    }
}


void UQuickSlotManager::OnInventoryRefreshed()
{
    OnInventorySlotUpdated(-1, nullptr);
}

bool UQuickSlotManager::UseQuickSlot(int32 QuickIndex)
{
    if (QuickIndex < 0 || QuickIndex >= NumSlots) return false;
    FQuickSlotEntry& E = Slots[QuickIndex];
    UInventoryComponent* Inv = E.Inventory.Get();
    UInventoryItem* Item = ResolveItem(QuickIndex);

    // 없으면 고스트 유지(슬롯/ItemId는 남겨둠)
    if (!Inv || !Item)
    {
        OnQuickSlotChanged.Broadcast(QuickIndex, nullptr); // 아이콘 회색/수량 0 갱신
        return false;
    }

    const int32 MaxSlots = Inv->GetSlotCount();
    for (int32 i = 0; i < MaxSlots; ++i)
    {
        if (UInventoryItem* It = Inv->GetItemAt(i))
        {
            if (It->InstanceId == E.ItemInstanceId)
            {
                const bool bUsed = UItemUseLibrary::UseConsumable(GetOwner(), Inv, i);
                if (bUsed)
                {
                    OnQuickSlotChanged.Broadcast(QuickIndex, ResolveItem(QuickIndex));
                }
                return bUsed;
            }
        }
    }

    // 인스턴스 못 찾으면 고스트 유지
    OnQuickSlotChanged.Broadcast(QuickIndex, nullptr);
    return false;
}

bool UQuickSlotManager::SwapSlots(int32 A, int32 B)
{
    if (A == B) return false;
    if (A < 0 || B < 0 || A >= NumSlots || B >= NumSlots) return false;

    FQuickSlotEntry& EA = Slots[A];
    FQuickSlotEntry& EB = Slots[B];
    Swap(EA, EB);

    // SlotIndex 갱신(사용 중이면)
    EA.SlotIndex = A;
    EB.SlotIndex = B;

    // 두 슬롯 모두 UI 갱신
    OnQuickSlotChanged.Broadcast(A, ResolveItem(A));
    OnQuickSlotChanged.Broadcast(B, ResolveItem(B));
    return true;
}

// 문자열/네임 소문자 비교
static bool EqualsIgnoreCase(const FString& A, const FString& B)
{
    return A.Equals(B, ESearchCase::IgnoreCase);
}

// Enum/Name/String 기반 "카테고리" 일치 확인 (PropName은 "ItemType" / "Type" / "Category" 순으로 시도)
static bool MatchEnumLikeCategory(const UObject* Obj, const TCHAR* PropName, const TArray<FName>& AllowedNames)
{
    if (!Obj) return false;
    if (FProperty* P = Obj->GetClass()->FindPropertyByName(PropName))
    {
        // 1) 진짜 enum 프로퍼티
        if (FEnumProperty* EP = CastField<FEnumProperty>(P))
        {
            const void* ValuePtr = EP->ContainerPtrToValuePtr<void>(Obj);
            const int64 Raw = EP->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
            const UEnum* En = EP->GetEnum();
            const FString EnumStr = En ? En->GetNameStringByValue(Raw) : FString();
            for (const FName& N : AllowedNames)
            {
                if (EqualsIgnoreCase(EnumStr, N.ToString())) return true; // "Consumable", "Quest", "Skill"
            }
        }
        // 2) byte enum (옛 API)
        else if (FByteProperty* BP = CastField<FByteProperty>(P))
        {
            const uint8* ValuePtr = BP->ContainerPtrToValuePtr<uint8>(Obj);
            const UEnum* En = BP->Enum;
            const FString EnumStr = En ? En->GetNameStringByValue(*ValuePtr) : FString();
            for (const FName& N : AllowedNames)
            {
                if (EqualsIgnoreCase(EnumStr, N.ToString())) return true;
            }
        }
        // 3) FName / FString로 카테고리를 들고있는 경우
        else if (FNameProperty* NP = CastField<FNameProperty>(P))
        {
            const FName Val = NP->GetPropertyValue_InContainer(Obj);
            for (const FName& N : AllowedNames)
            {
                if (Val.IsEqual(N, ENameCase::IgnoreCase)) return true;
            }
        }
        else if (FStrProperty* SP = CastField<FStrProperty>(P))
        {
            const FString Val = SP->GetPropertyValue_InContainer(Obj);
            for (const FName& N : AllowedNames)
            {
                if (EqualsIgnoreCase(Val, N.ToString())) return true;
            }
        }
    }
    return false;
}

// 불리언 플래그(bIsConsumable / bConsumable / bQuestItem / bIsSkill ...) 탐색
static bool MatchAnyBoolFlags(const UObject* Obj, const TArray<const TCHAR*>& Names)
{
    if (!Obj) return false;
    for (const TCHAR* PropName : Names)
    {
        if (FProperty* P = Obj->GetClass()->FindPropertyByName(PropName))
        {
            if (FBoolProperty* BP = CastField<FBoolProperty>(P))
            {
                if (BP->GetPropertyValue_InContainer(Obj)) return true;
            }
        }
    }
    return false;
}

bool UQuickSlotManager::IsAllowedForQuickslot(const UInventoryItem* Item) const
{
    if (!Item) return false;

    // 허용 목록을 "문자열 이름"으로 구성
    TArray<FName> Allowed;
    if (bAllowConsumables) Allowed.Add(TEXT("Consumable"));
    if (bAllowQuestItems)  Allowed.Add(TEXT("Quest"));
    if (bAllowSkills)      Allowed.Add(TEXT("Skill"));

    // 1) Enum/Name/String 기반 분류 시도 (가장 일반적인 케이스)
    for (const TCHAR* Prop : { TEXT("ItemType"), TEXT("Type"), TEXT("Category") })
    {
        if (MatchEnumLikeCategory(Item, Prop, Allowed))
        {
            return true;
        }
    }

    // 2) 불리언 플래그 기반 분류 시도 (bIsConsumable / bConsumable / bQuestItem / bIsQuest / bIsSkill ...)
    if (bAllowConsumables && MatchAnyBoolFlags(Item, { TEXT("bIsConsumable"), TEXT("bConsumable") })) return true;
    if (bAllowQuestItems && MatchAnyBoolFlags(Item, { TEXT("bQuestItem"), TEXT("bIsQuest"), TEXT("bQuest") })) return true;
    if (bAllowSkills && MatchAnyBoolFlags(Item, { TEXT("bIsSkill"), TEXT("bSkill") })) return true;

    // 3) 폴백: 소비 아이템은 보통 스택형 → 스택이면 "소비"로 간주(선택)
    if (bAllowConsumables && Item->IsStackable())
    {
        return true;
    }

    // 전부 실패 → 허용 불가
    return false;
}

int32 UQuickSlotManager::FindSlotByItemId(FName ItemId) const
{
    if (ItemId.IsNone()) return INDEX_NONE;
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        const FQuickSlotEntry& E = Slots[i];
        if (E.Inventory.IsValid() && E.ItemId == ItemId)
        {
            return i;
        }
    }
    return INDEX_NONE;
}

bool UQuickSlotManager::MoveSlot(int32 SourceIndex, int32 DestIndex)
{
    if (SourceIndex == DestIndex) return true;
    if (SourceIndex < 0 || DestIndex < 0 || SourceIndex >= NumSlots || DestIndex >= NumSlots) return false;

    FQuickSlotEntry& Src = Slots[SourceIndex];
    FQuickSlotEntry& Dst = Slots[DestIndex];

    // 목적지는 비어 있다고 가정(위젯 쪽에서만 호출). 안전장치:
    if (!Src.Inventory.IsValid()) return false;

    Dst = Src;
    Dst.SlotIndex = DestIndex;

    // 소스 비우기
    Src.Inventory = nullptr;
    Src.ItemInstanceId.Invalidate();
    Src.ItemId = NAME_None;

    // UI 갱신
    OnQuickSlotChanged.Broadcast(SourceIndex, ResolveItem(SourceIndex));
    OnQuickSlotChanged.Broadcast(DestIndex, ResolveItem(DestIndex));
    return true;
}

bool UQuickSlotManager::IsSlotAssigned(int32 QuickIndex) const
{
    return Slots.IsValidIndex(QuickIndex) && !Slots[QuickIndex].ItemId.IsNone();
}
