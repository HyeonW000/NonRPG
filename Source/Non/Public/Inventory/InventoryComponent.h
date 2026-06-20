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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoldChanged, int32, NewGold);

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

    /** [New] 골드가 변경되었을 때 UI 실시간 브로드캐스트용 델리게이트 */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Currency")
    FOnGoldChanged OnGoldChanged;

    /** [New] 소지 골드를 더해주는 안전 함수 (서버 권장) */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Currency")
    void AddGold(int32 Amount);

    /** [New] 소지 골드를 차감하는 함수 (서버 권장, 차감 성공 시 true 반환) */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Currency")
    bool RemoveGold(int32 Amount);

    /** [New] 현재 소지 골드를 반환하는 순수 함수 */
    UFUNCTION(BlueprintPure, Category = "Inventory|Currency")
    int32 GetGold() const { return Gold; }

    virtual void BeginPlay() override;
    UPROPERTY()
    TMap<FName, float> CooldownDurationByGroup; // GroupId -> DurationSeconds

    UFUNCTION(BlueprintPure) UInventoryItem* GetAt(int32 Index) const;
    UFUNCTION(BlueprintPure) int32 GetFirstEmptySlot() const;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(FName Rowname, int32 Quantity, int32& OutLastSlotIndex);

    /** [New] 여러 개의 아이템을 편리하게 한 번에 추가하는 디버그/치트용 함수 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    void AddMultipleItems(const TArray<FName>& ItemIds, int32 QuantityPerItem = 1);

    /** [New] 인벤토리 자동 정렬 및 빈 슬롯 압축 기능 */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SortInventory();

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

    /** [New] 가방 내 모든 아이템의 신규 획득 플래그 일괄 제거 */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void ClearAllNewItemFlags();

    // [Multiplayer]
    UPROPERTY(ReplicatedUsing = OnRep_ReplicatedSlots)
    TArray<FReplicatedInventorySlot> ReplicatedSlots;

    UFUNCTION()
    void OnRep_ReplicatedSlots();

    // [Multiplayer] 소비 아이템 사용 RPC
    UFUNCTION(Server, Reliable)
    void ServerUseConsumable(int32 SlotIndex);

    // ── [New] 상점 아이템 구매 요청 서버 RPC ──
    UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Inventory|Shop")
    void ServerBuyItem(FName ItemId, int32 Quantity);

    // ── [New] 상점 아이템 판매 요청 서버 RPC ──
    UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Inventory|Shop")
    void ServerSellItem(int32 SlotIndex, int32 Quantity);

    // [Multiplayer] 쿨다운 시작 클라이언트 알림
    UFUNCTION(Client, Reliable)
    void ClientPlayCooldown(FName GroupId, float Duration);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    bool IsValidIndex(int32 Index) const { return Slots.IsValidIndex(Index); }
    void BroadcastSlot(int32 Index);
    int32 FindStackableSlot(FName ItemId) const;

    /** [New] 멀티플레이어 서버 동기화(Replication)가 보증되는 골드 재화 변수 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Gold, meta = (AllowPrivateAccess = "true"), Category = "Inventory|Currency")
    int32 Gold = 0;

    UFUNCTION()
    void OnRep_Gold();
};
