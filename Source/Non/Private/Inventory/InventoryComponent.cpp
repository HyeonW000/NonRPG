#include "Inventory/InventoryComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h" // [Multiplayer]
#include "AbilitySystemComponent.h"

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true); // 컴포넌트 리플리케이션 활성화
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 내 인벤토리는 나만 봐야 함 (다른 클라이언트에게는 안 보냄)
    DOREPLIFETIME_CONDITION(UInventoryComponent, ReplicatedSlots, COND_OwnerOnly);
}

void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
    Slots.SetNum(MaxSlots);
    OnInventoryRefreshed.Broadcast();
    for (int32 i = 0; i < Slots.Num(); ++i) BroadcastSlot(i);
}

UInventoryItem* UInventoryComponent::GetAt(int32 Index) const
{
    return IsValidIndex(Index) ? Slots[Index] : nullptr;
}

int32 UInventoryComponent::GetFirstEmptySlot() const
{
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (Slots[i] == nullptr) return i;
    }
    return INDEX_NONE;
}

int32 UInventoryComponent::FindStackableSlot(FName ItemId) const
{
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (const UInventoryItem* It = Slots[i])
        {
            if (It->ItemId == ItemId && It->IsStackable() && It->Quantity < It->CachedRow.MaxStack)
            {
                return i;
            }
        }
    }
    return INDEX_NONE;
}

bool UInventoryComponent::AddItem(FName ItemId, int32 Quantity, int32& OutLastSlotIndex)
{
    OutLastSlotIndex = INDEX_NONE;
    
    if (!ItemDataTable) return false;
    if (ItemId.IsNone() || Quantity <= 0) return false;

    int32 Remaining = Quantity;
    while (Remaining > 0)
    {
        int32 StackSlot = FindStackableSlot(ItemId);
        if (StackSlot != INDEX_NONE)
        {
            UInventoryItem* Stk = Slots[StackSlot];
            const int32 MaxAdd = Stk->CachedRow.MaxStack - Stk->Quantity;
            const int32 ToAdd = FMath::Min(MaxAdd, Remaining);
            Stk->Quantity += ToAdd;
            Remaining -= ToAdd;
            BroadcastSlot(StackSlot);
            OutLastSlotIndex = StackSlot;
            continue;
        }

        const int32 Empty = GetFirstEmptySlot();
        if (Empty == INDEX_NONE) break;

        int32 MaxStack = 1;
        if (const FItemRow* Row = ItemDataTable->FindRow<FItemRow>(ItemId, TEXT("AddItem_MaxStack")))
            MaxStack = FMath::Max(1, Row->MaxStack);

        const int32 ToCreate = FMath::Min(MaxStack, Remaining);
        UInventoryItem* NewObj = NewObject<UInventoryItem>(this);
        NewObj->Init(ItemId, ToCreate, ItemDataTable);
        Slots[Empty] = NewObj;
        Remaining -= ToCreate;
        BroadcastSlot(Empty);
        OutLastSlotIndex = Empty;
    }

    return (Quantity - Remaining) > 0;
}

bool UInventoryComponent::RemoveAt(int32 Index, int32 Quantity)
{
    if (!IsValidIndex(Index) || !Slots[Index]) return false;

    if (Quantity >= Slots[Index]->Quantity)
    {
        Slots[Index] = nullptr;
        BroadcastSlot(Index);
        return true;
    }
    else
    {
        Slots[Index]->Quantity -= Quantity;
        BroadcastSlot(Index);
        return true;
    }
}

bool UInventoryComponent::Move(int32 From, int32 To)
{
    if (!IsValidIndex(From) || !IsValidIndex(To)) return false;
    if (From == To) return true;

    if (Slots[To] == nullptr)
    {
        Slots[To] = Slots[From];
        Slots[From] = nullptr;
        BroadcastSlot(From);
        BroadcastSlot(To);
        return true;
    }

    if (Slots[From] && Slots[To] &&
        Slots[From]->ItemId == Slots[To]->ItemId &&
        Slots[To]->IsStackable())
    {
        const int32 MaxStack = Slots[To]->CachedRow.MaxStack;
        const int32 Space = MaxStack - Slots[To]->Quantity;
        if (Space > 0)
        {
            const int32 ToMove = FMath::Min(Space, Slots[From]->Quantity);
            Slots[To]->Quantity += ToMove;
            Slots[From]->Quantity -= ToMove;
            if (Slots[From]->Quantity <= 0) Slots[From] = nullptr;
            BroadcastSlot(From);
            BroadcastSlot(To);
            return true;
        }
    }

    return Swap(From, To);
}

bool UInventoryComponent::Swap(int32 A, int32 B)
{
    if (!IsValidIndex(A) || !IsValidIndex(B) || A == B) return false;
    Slots.Swap(A, B);
    BroadcastSlot(A);
    BroadcastSlot(B);
    return true;
}

bool UInventoryComponent::SplitStack(int32 From, int32 To, int32 MoveQty)
{
    if (!IsValidIndex(From) || !IsValidIndex(To)) return false;
    if (From == To || MoveQty <= 0) return false;

    UInventoryItem* Src = Slots[From];
    if (!Src || !Src->IsStackable() || Src->Quantity <= 1) return false;

    if (Slots[To] == nullptr)
    {
        const int32 Actual = FMath::Clamp(MoveQty, 1, Src->Quantity - 1);
        UInventoryItem* NewObj = NewObject<UInventoryItem>(this);
        NewObj->Init(Src->ItemId, Actual, ItemDataTable);
        Slots[To] = NewObj;
        Src->Quantity -= Actual;
        BroadcastSlot(From);
        BroadcastSlot(To);
        return true;
    }

    return Move(From, To);
}

void UInventoryComponent::Refresh()
{
    Slots.SetNum(MaxSlots);
    OnInventoryRefreshed.Broadcast();
    for (int32 i = 0; i < Slots.Num(); ++i) BroadcastSlot(i);
}

void UInventoryComponent::BroadcastSlot(int32 Index)
{
    if (IsValidIndex(Index))
    {
        // [Multiplayer] 서버에서 변경 시 리플리케이션 배열 동기화
        if (GetOwner()->HasAuthority())
        {
            // 리플리케이션 배열에서 해당 슬롯 찾기
            int32 FoundIdx = ReplicatedSlots.IndexOfByPredicate([Index](const FReplicatedInventorySlot& Entry) {
                return Entry.SlotIndex == Index;
            });

            UInventoryItem* CurrentItem = Slots[Index];

            if (CurrentItem)
            {
                // 아이템이 있으면: 업데이트 or 추가
                FReplicatedInventorySlot NewData;
                NewData.SlotIndex = Index;
                NewData.ItemId = CurrentItem->ItemId;
                NewData.Quantity = CurrentItem->Quantity;

                if (FoundIdx != INDEX_NONE)
                {
                    ReplicatedSlots[FoundIdx] = NewData;
                }
                else
                {
                    ReplicatedSlots.Add(NewData);
                }
            }
            else
            {
                // 아이템이 없으면: 제거
                if (FoundIdx != INDEX_NONE)
                {
                    ReplicatedSlots.RemoveAt(FoundIdx);
                }
            }
        }

        OnSlotUpdated.Broadcast(Index, Slots[Index]);
    }
}

void UInventoryComponent::OnRep_ReplicatedSlots()
{
    // 리플리케이션 데이터 수신 시, 로컬 Slots 배열을 재구성
    
    // 1. 일단 싹 비우기 (MaxSlots 유지)
    // (주의: 드래그 중인 아이템이 있으면 곤란할 수 있지만, 여기서는 단순화)
    Slots.Init(nullptr, MaxSlots);

    // 2. 복원
    if (!ItemDataTable) return;

    for (const FReplicatedInventorySlot& Data : ReplicatedSlots)
    {
        if (Slots.IsValidIndex(Data.SlotIndex))
        {
            UInventoryItem* NewItem = NewObject<UInventoryItem>(this);
            NewItem->Init(Data.ItemId, Data.Quantity, ItemDataTable);
            Slots[Data.SlotIndex] = NewItem;
        }
    }

    // 3. UI 갱신 알림
    OnInventoryRefreshed.Broadcast();
}

void UInventoryComponent::DumpInventoryOnScreen() const
{
    if (!GEngine) return;
    if (!GEngine) return;

}

bool UInventoryComponent::GetCooldownRemaining(FName GroupId, float& OutRemaining, float& OutTotal) const
{
    OutRemaining = 0.f;
    OutTotal = 0.f;

    if (GroupId.IsNone() || !GetWorld()) return false;

    const float* EndPtr = CooldownEndTimeByGroup.Find(GroupId);
    const float* DurPtr = CooldownDurationByGroup.Find(GroupId);
    if (!EndPtr || !DurPtr) return false;

    const float Now = GetWorld()->GetTimeSeconds();
    OutTotal = FMath::Max(0.f, *DurPtr);
    OutRemaining = FMath::Max(0.f, *EndPtr - Now);

    return (OutRemaining > 0.f && OutTotal > 0.f);
}

bool UInventoryComponent::IsValidIndex_Public(int32 Index) const
{
    const int32 Count = GetSlotCount();
    return (Index >= 0 && Index < Count);
}

UInventoryItem* UInventoryComponent::GetItemAt(int32 Index) const
{
    if (!IsValidIndex_Public(Index)) return nullptr;

    // 여기서도 동일하게 실제 배열 이름 사용
    return Slots[Index];
}

int32 UInventoryComponent::GetSlotCount() const
{
    return Slots.Num();
}

bool UInventoryComponent::RemoveAllAt(int32 Index)
{
    if (!IsValidIndex(Index) || !Slots[Index]) return false;
    const int32 AllQty = Slots[Index]->Quantity;
    return RemoveAt(Index, AllQty);
}

bool UInventoryComponent::IsCooldownActive(FName GroupId) const
{
    if (GroupId.IsNone() || !GetWorld()) return false;
    const float* EndPtr = CooldownEndTimeByGroup.Find(GroupId);
    const float Now = GetWorld()->GetTimeSeconds();
    return (EndPtr && *EndPtr > Now);
}

void UInventoryComponent::StartCooldown(FName GroupId, float Duration)
{
    if (GroupId.IsNone() || Duration <= 0.f || !GetWorld()) return;
    const float Now = GetWorld()->GetTimeSeconds();
    const float EndTime = Now + Duration;
    
    CooldownEndTimeByGroup.Add(GroupId, EndTime);
    CooldownDurationByGroup.Add(GroupId, Duration);

    OnCooldownStarted.Broadcast(GroupId, Duration, EndTime);
}

TArray<FInventorySaveData> UInventoryComponent::GetItemsForSave() const
{
    TArray<FInventorySaveData> OutData;
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (const UInventoryItem* It = Slots[i])
        {
            FInventorySaveData Data;
            Data.ItemId = It->ItemId;
            Data.Quantity = It->Quantity;
            OutData.Add(Data);
        }
    }
    return OutData;
}

void UInventoryComponent::RestoreItemsFromSave(const TArray<FInventorySaveData>& InData)
{
    // 1. 기존 아이템 클리어
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        Slots[i] = nullptr;
        BroadcastSlot(i);
    }
    
    // 2. 저장된 데이터로 아이템 복구
    for (const FInventorySaveData& Data : InData)
    {
        // AddItem 로직 재사용 (빈 슬롯 찾아 추가)
        int32 Dummy;
        AddItem(Data.ItemId, Data.Quantity, Dummy);
    }
}
void UInventoryComponent::ServerUseConsumable_Implementation(int32 SlotIndex)
{
    if (!IsValidIndex(SlotIndex) || !Slots[SlotIndex]) return;

    UInventoryItem* It = Slots[SlotIndex];
    if (It->CachedRow.ItemType != EItemType::Consumable) return;

    // 쿨다운 체크
    const FName GroupId = It->CachedRow.Consumable.CooldownGroupId;
    if (!GroupId.IsNone() && IsCooldownActive(GroupId))
    {
        return; // 실패
    }

    // Effect 적용 (ASC GE or Function)
    // 여기서는 ItemUseLibrary가 하던 일을 서버에서 수행
    if (AActor* OwnerActor = GetOwner())
    {
        if (UAbilitySystemComponent* ASC = OwnerActor->FindComponentByClass<UAbilitySystemComponent>())
        {
            // 예: GE 적용 (필요하다면 ItemRow에 정의된 GE 클래스를 사용)
            // (간단 구현: GE 없이 로그만)

        }
    }

    // 수량 차감
    RemoveAt(SlotIndex, 1);

    // 쿨다운 시작 & 동기화 (쿨다운은 ReplicatedSlots에 없으므로 별도 RPC나 리플리케이션 필요)
    // 여기서는 간단히 로컬(서버) 적용만 하고, Client는 Prediction으로 UI 타이머 돌림
    // (완벽한 동기화를 위해선 쿨다운 정보를 리플리케이션 해야 함. CooldownEndTimeByGroup를 Replicated로?)
    if (!GroupId.IsNone() && It->CachedRow.Consumable.CooldownTime > 0.f)
    {
        // 서버도 쿨다운 관리 (보안/검증용)
        StartCooldown(GroupId, It->CachedRow.Consumable.CooldownTime);
        // 클라이언트에게 알림 (UI 표시용)
        ClientPlayCooldown(GroupId, It->CachedRow.Consumable.CooldownTime);
    }
}

void UInventoryComponent::ClientPlayCooldown_Implementation(FName GroupId, float Duration)
{
    // 클라이언트 로컬 타이머 시작 -> Delegate Broadcast -> UI 갱신
    StartCooldown(GroupId, Duration);
}
