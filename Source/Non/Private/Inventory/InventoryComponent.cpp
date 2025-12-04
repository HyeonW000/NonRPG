#include "Inventory/InventoryComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
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
    if (!ItemDataTable || ItemId.IsNone() || Quantity <= 0) return false;

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
        OnSlotUpdated.Broadcast(Index, Slots[Index]);
    }
}

bool UInventoryComponent::IsCooldownActive(FName GroupId) const
{
    if (GroupId.IsNone() || !GetWorld()) return false;
    const float* EndPtr = CooldownEndTimeByGroup.Find(GroupId);
    const float Now = GetWorld()->GetTimeSeconds();
    return (EndPtr && *EndPtr > Now);
}


bool UInventoryComponent::RemoveAllAt(int32 Index)
{
    // 실질적으로 '전부' 제거하려면 현재 수량만큼 제거
    if (!Slots.IsValidIndex(Index) || !Slots[Index]) return false;
    const int32 AllQty = Slots[Index]->Quantity;
    return RemoveAt(Index, AllQty);
}

void UInventoryComponent::DumpInventoryOnScreen() const
{
    if (!GEngine) return;
    GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan, TEXT("=== Inventory Dump ==="));
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        const UInventoryItem* It = Slots[i];
        const FString Line = FString::Printf(TEXT("[%02d] %s x%d"),
            i,
            It ? *It->ItemId.ToString() : TEXT("Empty"),
            It ? It->Quantity : 0);
        GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan, Line);
    }
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

void UInventoryComponent::StartCooldown(FName GroupId, float CooldownSeconds)
{
    if (GroupId.IsNone() || !GetWorld() || CooldownSeconds <= 0.f) return;
    const float Now = GetWorld()->GetTimeSeconds();
    CooldownEndTimeByGroup.Add(GroupId, Now + CooldownSeconds);
    CooldownDurationByGroup.Add(GroupId, CooldownSeconds); // (옵션) 총 길이 저장
}

int32 UInventoryComponent::GetSlotCount() const
{
    // 여기서 실제 보유 배열 이름으로 교체: Items 또는 Slots 등
    return Slots.Num();
    // 만약 MaxSlots만 있고 배열은 동적이 아니라면: return MaxSlots;
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
