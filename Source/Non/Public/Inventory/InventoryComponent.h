#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/InventoryItem.h"
#include "Data/ItemStructs.h"
#include "Inventory/ItemEnums.h"
#include "System/NonSaveGame.h"  // FInventorySaveData
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventorySlotUpdated, int32, SlotIndex, UInventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryRefreshed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInventoryCooldownStarted, FName, GroupId, float, Duration, float, EndTime);

// [Multiplayer] 리플리케이션용 구조체
USTRUCT(BlueprintType)
struct FReplicatedInventorySlot
{
    GENERATED_BODY()

    UPROPERTY() int32 SlotIndex = INDEX_NONE;
    UPROPERTY() FName ItemId = NAME_None;
    UPROPERTY() int32 Quantity = 0;

    // 필요시 인챈트 정보 등 추가

    bool operator==(const FReplicatedInventorySlot& Other) const
    {
        return SlotIndex == Other.SlotIndex && ItemId == Other.ItemId && Quantity == Other.Quantity;
    }
};

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
    UPROPERTY(BlueprintAssignable) FOnInventoryCooldownStarted OnCooldownStarted;

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

    /** 저장용: 전체 아이템 리스트 반환 (빈 슬롯 제외) */
    TArray<FInventorySaveData> GetItemsForSave() const;

    /** 로드용: 저장 데이터로 인벤토리 복구 */
    void RestoreItemsFromSave(const TArray<FInventorySaveData>& InData);

    /** 인벤토리 비우기 */
    void ClearAll();

    // [Multiplayer]
    UPROPERTY(ReplicatedUsing = OnRep_ReplicatedSlots)
    TArray<FReplicatedInventorySlot> ReplicatedSlots;

    UFUNCTION()
    void OnRep_ReplicatedSlots();

    // [Multiplayer] 소비 아이템 사용 RPC
    UFUNCTION(Server, Reliable)
    void ServerUseConsumable(int32 SlotIndex);

    // [Multiplayer] 쿨다운 시작 클라이언트 알림
    UFUNCTION(Client, Reliable)
    void ClientPlayCooldown(FName GroupId, float Duration);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    bool IsValidIndex(int32 Index) const { return Slots.IsValidIndex(Index); }
    void BroadcastSlot(int32 Index);
    int32 FindStackableSlot(FName ItemId) const;
};
