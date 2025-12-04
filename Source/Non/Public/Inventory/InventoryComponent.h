#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/InventoryItem.h"
#include "Data/ItemStructs.h"
#include "Inventory/ItemEnums.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventorySlotUpdated, int32, SlotIndex, UInventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryRefreshed);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class NON_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UInventoryComponent();

    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    void DumpInventoryOnScreen() const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 MaxSlots = 40;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    TObjectPtr<UDataTable> ItemDataTable;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TArray<TObjectPtr<UInventoryItem>> Slots;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetSlotCount() const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool IsValidIndex_Public(int32 Index) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    UInventoryItem* GetItemAt(int32 Index) const;

    // 간단 쿨다운 그룹 트래킹(소비 아이템용)
    UPROPERTY()
    TMap<FName, float> CooldownEndTimeByGroup; // GroupId -> EndTimeSeconds

    UPROPERTY(BlueprintAssignable) FOnInventorySlotUpdated OnSlotUpdated;
    UPROPERTY(BlueprintAssignable) FOnInventoryRefreshed   OnInventoryRefreshed;

    virtual void BeginPlay() override;
    UPROPERTY()
    TMap<FName, float> CooldownDurationByGroup; // GroupId -> DurationSeconds

    UFUNCTION(BlueprintPure) UInventoryItem* GetAt(int32 Index) const;
    UFUNCTION(BlueprintPure) int32 GetFirstEmptySlot() const;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(FName Rowname, int32 Quantity, int32& OutLastSlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveAt(int32 Index, int32 Quantity);

    // 전부 제거용 래퍼(블루프린트에서 쓰기 좋게 별도 노드 제공)
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveAllAt(int32 Index);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool Move(int32 From, int32 To);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool Swap(int32 A, int32 B);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool SplitStack(int32 From, int32 To, int32 MoveQty);


    UFUNCTION(BlueprintCallable, Category = "Inventory|Cooldown")
    bool GetCooldownRemaining(FName GroupId, float& OutRemaining, float& OutTotal) const;

    UFUNCTION(BlueprintCallable) void Refresh();

    // 쿨다운 그룹
    UFUNCTION(BlueprintPure)  bool IsCooldownActive(FName GroupId) const;
    UFUNCTION(BlueprintCallable) void StartCooldown(FName GroupId, float CooldownSeconds);
private:
    bool IsValidIndex(int32 Index) const { return Slots.IsValidIndex(Index); }
    void BroadcastSlot(int32 Index);
    int32 FindStackableSlot(FName ItemId) const;
};
