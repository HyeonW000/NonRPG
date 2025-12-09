#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/ItemTypes.h"          // FItemData (Row) 구조
#include "Inventory/ItemEnums.h"
#include "Inventory/InventoryItem.h"
#include "EquipmentComponent.generated.h"

class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEquipped, EEquipmentSlot, Slot, UInventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnequipped, EEquipmentSlot, Slot);

USTRUCT(BlueprintType)
struct FEquipmentVisual
{
    GENERATED_BODY()

    // 기본적으로 아이템 Row(=FItemData)의 AttachSocket을 쓰되, 슬롯별로 덮어쓸 수 있게 함
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Socket = NAME_None;

    // 소켓 기준 오프셋(상하/좌우/전후 및 회전/스케일)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform Relative = FTransform::Identity;

    // 장착 시 가시성/충돌 옵션
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bOwnerNoCollision = true;
};

/**
 * 인벤토리 아이템을 장비로 장착/해제하고,
 * 실제 캐릭터 메시에 메시 컴포넌트를 붙여 비주얼을 보여주는 컴포넌트
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class NON_API UEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEquipmentComponent();

    UFUNCTION(BlueprintPure, Category = "Equip")
    bool IsMainHandOccupyingBothHands() const; // 메인핸드가 양손(또는 스태프)인지

    UFUNCTION(BlueprintPure, Category = "Equip")
    bool IsTwoHandedLike(const UInventoryItem* Item) const; // TwoHanded + Staff


    // 인벤토리에서 SlotIndex 아이템을 꺼내 장비 슬롯에 장착 (OptionalTarget이 None이면 아이템의 Row.EquipSlot 사용)
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool EquipFromInventory(UInventoryComponent* Inv, int32 FromIndex, EEquipmentSlot OptionalTarget = EEquipmentSlot::None);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool Unequip(EEquipmentSlot Slot);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    UInventoryItem* GetEquippedItemBySlot(EEquipmentSlot Slot) const;

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool UnequipToInventory(EEquipmentSlot Slot, int32& OutInventoryIndex);

    // 드래그 드롭 등에서 아이템만 받아 장착하고 싶을 때
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool EquipItem(UInventoryItem* Item, EEquipmentSlot OptionalTarget = EEquipmentSlot::None);

    // 장착/해제 브로드캐스트 (UI 반영용)
    UPROPERTY(BlueprintAssignable) FOnEquipped OnEquipped;
    UPROPERTY(BlueprintAssignable) FOnUnequipped OnUnequipped;

    // 슬롯 → 장착된 아이템
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
    TMap<EEquipmentSlot, TObjectPtr<UInventoryItem>> Equipped;

    // 슬롯 → 비주얼 세팅 (에디터에서 슬롯별 소켓/오프셋 지정 가능)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Visual")
    TMap<EEquipmentSlot, FEquipmentVisual> VisualSlots;

    /** 현재 생성된 비주얼(메시 컴포넌트)을 다른 소켓으로 재부착 */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Visual", meta = (AutoCreateRefTerm = "Relative"))
    bool ReattachSlotToSocket(EEquipmentSlot Slot, FName NewSocket, const FTransform& Relative);

    /** 트레이스 등에 쓰기 위해, 슬롯의 실제 UMeshComponent를 가져온다(없으면 nullptr) */
    UFUNCTION(BlueprintPure, Category = "Equipment|Visual")
    UMeshComponent* GetVisualComponent(EEquipmentSlot Slot) const;


    USceneComponent* GetVisualForSlot(EEquipmentSlot Slot) const;

    // (1) 기본 시스(등/허리) 소켓 — BP에서 바꿔도 됨
    UPROPERTY(EditDefaultsOnly, Category = "Equipment|Defaults")
    FName DefaultSheathSocket1H = TEXT("sheath_hip_r");

    UPROPERTY(EditDefaultsOnly, Category = "Equipment|Defaults")
    FName DefaultSheathSocket2H = TEXT("sheath_back");

    // (2) 슬롯별 홈소켓(=Unarmed 복귀 목적지) 캐시
    UPROPERTY(Transient)
    TMap<EEquipmentSlot, FName> SlotHomeSocketMap;

    // (3) 홈소켓 셋업/조회/재계산 함수
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void SetHomeSocketForSlot(EEquipmentSlot Slot, FName SocketName);

    UFUNCTION(BlueprintPure, Category = "Equipment")
    FName GetHomeSocketForSlot(EEquipmentSlot Slot) const;

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void RecomputeHomeSocketFromEquipped(EEquipmentSlot Slot);

    // (선택) 지금 슬롯을 홈소켓으로 재부착
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void ReattachSlotToHome(EEquipmentSlot Slot);


protected:
    virtual void BeginPlay() override;

    // 규칙 체크(예: 무기 타입 등). 실패 시 OutFailReason 채움
    bool CanEquip(const UInventoryItem* Item, EEquipmentSlot TargetSlot, FText& OutFailReason) const;

    // 내부 장착/해제
    bool EquipInternal(UInventoryItem* Item, EEquipmentSlot TargetSlot, int32* OutReturnedIndex = nullptr);
    void UnequipInternal(EEquipmentSlot Slot);

    // 충돌 규칙(예: 양손 무기와 보조무기 동시 장착 불가 등) 정리
    void ResolveTwoHandedConflicts();
    void ResolveWeaponSubConflicts();

    // === 비주얼 ===
    // 슬롯에 메시 컴포넌트를 붙임 (기존 있으면 제거 후 재생성)
    void ApplyVisual(EEquipmentSlot Slot, const FItemRow& Row);
    void RemoveVisual(EEquipmentSlot Slot);

    // 장착/해제 시 옵션 훅(원하면 GAS 효과 적용/해제에 사용)
    void ApplyEquipmentEffects(const FItemRow& Row);
    void RemoveEquipmentEffects(const FItemRow& Row);
    void RecomputeSetBonuses();

    // 소유자 메인 메쉬(캐릭터면 GetMesh())
    USkeletalMeshComponent* GetOwnerMesh() const;

private:
    UPROPERTY(Transient)
    TObjectPtr<UInventoryComponent> OwnerInventory = nullptr;

    // 슬롯 → 생성된 메시 컴포넌트 (Skeletal 또는 Static)
    UPROPERTY(Transient)
    TMap<EEquipmentSlot, TObjectPtr<UMeshComponent>> VisualComponents;

public:
    // [SaveSystem]
    TArray<struct FEquipmentSaveData> GetEquippedItemsForSave() const;
    void RestoreEquippedItemsFromSave(const TArray<struct FEquipmentSaveData>& InData);
};
