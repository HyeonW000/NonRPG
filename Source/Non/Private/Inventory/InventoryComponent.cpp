#include "Inventory/InventoryComponent.h"
#include "Character/NonCharacterBase.h"
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

    // [Multiplayer Fix] 클라이언트로 인벤토리 슬롯 및 골드 복제 보장
    DOREPLIFETIME(UInventoryComponent, ReplicatedSlots);
    DOREPLIFETIME(UInventoryComponent, Gold);
}

void UInventoryComponent::EnsureDataTableLoaded()
{
    if (!ItemDataTable && GetOwner())
    {
        if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetOwner()))
        {
            ItemDataTable = Char->DefaultItemDataTable;
        }
    }
}

void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
    EnsureDataTableLoaded();
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

    // [Multiplayer Fix] 클라이언트에서 AddItem 호출 시 서버 RPC로 완벽 포워딩!
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetOwner()))
        {
            Char->ServerAddItem(ItemId, Quantity);
        }
        else
        {
            ServerAddItem(ItemId, Quantity);
        }
        return true;
    }
    
    EnsureDataTableLoaded();

    if (!ItemDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [Inventory] ItemDataTable is nullptr! DefaultItemDataTable이 캐릭터에 설정되어 있는지 확인하세요."));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, 
                TEXT("❌ [Inventory] ItemDataTable이 비어있습니다! BP_PlayerCharacter의 DefaultItemDataTable을 확인하세요."));
        }
        return false;
    }

    if (ItemId.IsNone() || Quantity <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ [Inventory] 유효하지 않은 아이템 요청: ItemId=%s, Quantity=%d"), *ItemId.ToString(), Quantity);
        return false;
    }

    // 데이터 테이블에 해당 아이템 행이 존재하는지 사전 검증
    if (!ItemDataTable->FindRow<FItemRow>(ItemId, TEXT("AddItem_Check")))
    {
        TArray<FName> AllRows = ItemDataTable->GetRowNames();
        FString Available = FString::JoinBy(AllRows, TEXT(", "), [](const FName& N) { return N.ToString(); });
        UE_LOG(LogTemp, Error, TEXT("❌ [Inventory] '%s' 아이템을 DataTable에서 찾을 수 없습니다! 사용 가능한 아이템 목록: [%s]"), *ItemId.ToString(), *Available);
        
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
                FString::Printf(TEXT("❌ [Inventory] '%s' 아이템 없음! 사용 가능: [%s]"), *ItemId.ToString(), *Available));
        }
        return false;
    }

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
            
            // [New] 기존 슬롯 수량 누적 시에도 신규 획득 플래그를 활성화합니다.
            Stk->bIsNewItem = true;

            Remaining -= ToAdd;
            BroadcastSlot(StackSlot);
            OutLastSlotIndex = StackSlot;

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, 
                    FString::Printf(TEXT("✅ [Inventory] '%s' x%d 수량 누적 성공 (슬롯 %d)"), *ItemId.ToString(), ToAdd, StackSlot));
            }
            continue;
        }

        const int32 Empty = GetFirstEmptySlot();
        if (Empty == INDEX_NONE)
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("❌ [Inventory] 가방이 꽉 찼습니다!"));
            }
            break;
        }

        int32 MaxStack = 1;
        if (const FItemRow* Row = ItemDataTable->FindRow<FItemRow>(ItemId, TEXT("AddItem_MaxStack")))
            MaxStack = FMath::Max(1, Row->MaxStack);

        const int32 ToCreate = FMath::Min(MaxStack, Remaining);
        UInventoryItem* NewObj = NewObject<UInventoryItem>(this);
        NewObj->Init(ItemId, ToCreate, ItemDataTable);

        // [New] 신규 슬롯 생성 획득 시 신규 획득 플래그를 활성화합니다.
        NewObj->bIsNewItem = true;

        Slots[Empty] = NewObj;
        Remaining -= ToCreate;
        BroadcastSlot(Empty);
        OutLastSlotIndex = Empty;

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, 
                FString::Printf(TEXT("✅ [Inventory] '%s' x%d 새 슬롯에 추가 완료! (슬롯 %d)"), *ItemId.ToString(), ToCreate, Empty));
        }
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

            // 클라이언트 화면으로 즉시 강제 갱신 RPC 전송
            Client_ForceRefreshInventory();
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
    EnsureDataTableLoaded();
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

    // 3. UI 갱신 알림 (전체 리프레시 + 슬롯별 업데이트)
    OnInventoryRefreshed.Broadcast();
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        OnSlotUpdated.Broadcast(i, Slots[i]);
    }
}

void UInventoryComponent::Client_ForceRefreshInventory_Implementation()
{
    OnRep_ReplicatedSlots();
}

void UInventoryComponent::DumpInventoryOnScreen() const
{
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

void UInventoryComponent::OnRep_Gold()
{
    // [New] 서버로부터 갱신된 골드 수치를 수신하여 로컬 UI 감청자들에게 전파합니다.
    OnGoldChanged.Broadcast(Gold);
}

void UInventoryComponent::AddGold(int32 Amount)
{
    if (Amount <= 0) return;

    // 골드 획득 제어는 서버(Authority)에서만 엄격히 통제합니다. (클라 임의 해킹 방지)
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    Gold = FMath::Max(0, Gold + Amount);
    OnGoldChanged.Broadcast(Gold);
}

bool UInventoryComponent::RemoveGold(int32 Amount)
{
    if (Amount <= 0) return false;
    if (Gold < Amount) return false; // 잔액 부족

    // 골드 차감 제어도 서버(Authority)에서만 엄격히 통제합니다.
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;

    Gold = FMath::Max(0, Gold - Amount);
    OnGoldChanged.Broadcast(Gold);
    return true;
}

void UInventoryComponent::ServerAddItem_Implementation(FName ItemId, int32 Quantity)
{
    int32 DummyIndex = INDEX_NONE;
    AddItem(ItemId, Quantity, DummyIndex);
}

void UInventoryComponent::AddMultipleItems(const TArray<FName>& ItemIds, int32 QuantityPerItem)
{
    if (ItemIds.Num() == 0 || QuantityPerItem <= 0)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, 
                FString::Printf(TEXT("⚠️ [Inventory] AddMultipleItems: ItemIds 배열이 비어있거나 수량(%d)이 0 이하입니다!"), QuantityPerItem));
        }
        return;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, 
            FString::Printf(TEXT("📦 [Inventory] AddMultipleItems 요청 수신: %d개 품목 (각 %d개)"), ItemIds.Num(), QuantityPerItem));
    }

    // [Multiplayer Fix] 클라이언트에서 AddMultipleItems 호출 시 서버 RPC로 완벽 포워딩!
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetOwner()))
        {
            Char->ServerAddMultipleItems(ItemIds, QuantityPerItem);
        }
        else
        {
            ServerAddMultipleItems(ItemIds, QuantityPerItem);
        }
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("📦 [Inventory] AddMultipleItems 실행: %d개 품목 요청 (Owner: %s, HasAuth: %d)"), 
        ItemIds.Num(), *GetNameSafe(GetOwner()), GetOwner() ? GetOwner()->HasAuthority() : 0);

    for (const FName& ItemId : ItemIds)
    {
        int32 DummyIndex;
        AddItem(ItemId, QuantityPerItem, DummyIndex);
    }
}

void UInventoryComponent::ServerAddMultipleItems_Implementation(const TArray<FName>& ItemIds, int32 QuantityPerItem)
{
    AddMultipleItems(ItemIds, QuantityPerItem);
}

void UInventoryComponent::SortInventory()
{
    // 정렬과 압축은 서버 권한(Authority) 하에서만 안전하게 실행하고 리플리케이션 처리해야 합니다.
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    // ── 1단계: 스택 합치기 (Consolidate Stacks) ──────────────────────────
    // 동일한 아이템이 쪼개져 있는 경우, 앞에서부터 꽉꽉 채워 합칩니다.
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        UInventoryItem* Current = Slots[i];
        if (!Current || !Current->IsStackable()) continue;

        // 이미 스택이 꽉 차 있으면 패스
        if (Current->Quantity >= Current->GetMaxStack()) continue;

        // 현재 슬롯의 뒷 슬롯들 중에서 합칠 수 있는 동일한 아이템 탐색
        for (int32 j = i + 1; j < Slots.Num(); ++j)
        {
            UInventoryItem* Next = Slots[j];
            if (!Next || Next->ItemId != Current->ItemId) continue;

            const int32 MaxStack = Current->GetMaxStack();
            const int32 Space = MaxStack - Current->Quantity;
            const int32 ToMove = FMath::Min(Space, Next->Quantity);

            Current->Quantity += ToMove;
            Next->Quantity -= ToMove;

            if (Next->Quantity <= 0)
            {
                Slots[j] = nullptr;
            }

            if (Current->Quantity >= MaxStack) break; // 현재 슬롯이 꽉 차면 탐색 중단
        }
    }

    // ── 2단계: 유효한 아이템만 모으기 ───────────────────────────
    TArray<UInventoryItem*> ValidItems;
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (Slots[i] != nullptr)
        {
            ValidItems.Add(Slots[i]);
        }
    }

    // ── 3단계: 정교한 기준에 맞추어 정렬 (Sort) ─────────────────────────
    ValidItems.Sort([](const UInventoryItem& A, const UInventoryItem& B) {
        // 1순위: 아이템 타입 오름차순 (Equipment -> Consumable -> Material -> Quest ...)
        if (A.CachedRow.ItemType != B.CachedRow.ItemType)
        {
            return static_cast<uint8>(A.CachedRow.ItemType) < static_cast<uint8>(B.CachedRow.ItemType);
        }

        // [New] 1-2순위: 대분류가 '장비(Equipment)'로 동일한 경우, 세부 장착 부위별 우선순위 비교 (무기 최우선!)
        if (A.CachedRow.ItemType == EItemType::Equipment)
        {
            auto GetEquipSlotPriority = [](EEquipmentSlot Slot) -> int32 {
                switch (Slot)
                {
                    case EEquipmentSlot::WeaponMain: return 0; // 무기 최우선
                    case EEquipmentSlot::WeaponSub:  return 1; // 보조 무기
                    case EEquipmentSlot::Head:       return 2; // 방어구
                    case EEquipmentSlot::Chest:      return 3;
                    case EEquipmentSlot::Legs:       return 4;
                    case EEquipmentSlot::Hands:      return 5;
                    case EEquipmentSlot::Feet:       return 6;
                    case EEquipmentSlot::Accessory1: return 7; // 액세서리
                    case EEquipmentSlot::Accessory2: return 8;
                    default:                         return 9; // 기타
                }
            };

            int32 PriorityA = GetEquipSlotPriority(A.CachedRow.EquipSlot);
            int32 PriorityB = GetEquipSlotPriority(B.CachedRow.EquipSlot);

            if (PriorityA != PriorityB)
            {
                return PriorityA < PriorityB;
            }
        }

        // 2순위: 등급 내림차순 (전설/신화 등이 먼저 오도록)
        if (A.CachedRow.Rarity != B.CachedRow.Rarity)
        {
            return static_cast<uint8>(A.CachedRow.Rarity) > static_cast<uint8>(B.CachedRow.Rarity);
        }

        // 3순위: 이름(ItemId) 사전 오름차순
        if (A.ItemId != B.ItemId)
        {
            return A.ItemId.ToString() < B.ItemId.ToString();
        }

        // 4순위: 소지 수량 내림차순
        return A.Quantity > B.Quantity;
    });

    // ── 4단계: 슬롯 재배치 및 리플리케이션 갱신 ──────────────────────────
    // 모든 슬롯을 비워주고 앞부터 정렬된 아이템을 차례로 채웁니다.
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (i < ValidItems.Num())
        {
            Slots[i] = ValidItems[i];
        }
        else
        {
            Slots[i] = nullptr;
        }
    }

    // 서버의 리플리케이션 데이터(ReplicatedSlots) 전체 재구성 및 BroadcastSlot
    ReplicatedSlots.Empty();
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        BroadcastSlot(i);
    }

    // UI에 전체 리프레시 델리게이트 송출
    OnInventoryRefreshed.Broadcast();
}

void UInventoryComponent::ClearAllNewItemFlags()
{
    bool bChanged = false;
    for (UInventoryItem* It : Slots)
    {
        if (It && It->bIsNewItem)
        {
            It->bIsNewItem = false;
            bChanged = true;
        }
    }

    if (bChanged)
    {
        Refresh();
    }
}

bool UInventoryComponent::ServerBuyItem_Validate(FName ItemId, int32 Quantity)
{
    return !ItemId.IsNone() && Quantity > 0;
}

void UInventoryComponent::ServerBuyItem_Implementation(FName ItemId, int32 Quantity)
{
    if (!ItemDataTable) return;

    const FItemRow* ItemRow = ItemDataTable->FindRow<FItemRow>(ItemId, TEXT("ShopBuyCheck"));
    if (!ItemRow) return;

    const int32 TotalCost = ItemRow->BuyPrice * Quantity;
    if (Gold < TotalCost) return;

    int32 OutLastSlot = INDEX_NONE;
    if (AddItem(ItemId, Quantity, OutLastSlot))
    {
        RemoveGold(TotalCost);
    }
}

bool UInventoryComponent::ServerSellItem_Validate(int32 SlotIndex, int32 Quantity)
{
    return IsValidIndex(SlotIndex) && Quantity > 0;
}

void UInventoryComponent::ServerSellItem_Implementation(int32 SlotIndex, int32 Quantity)
{
    if (!ItemDataTable) return;

    UInventoryItem* TargetItem = GetItemAt(SlotIndex);
    if (!TargetItem) return;

    if (TargetItem->Quantity < Quantity) return;

    const int32 TotalReward = TargetItem->CachedRow.SellPrice * Quantity;

    if (RemoveAt(SlotIndex, Quantity))
    {
        AddGold(TotalReward);
    }
}
