#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/ItemEnums.h"
#include "CharacterWindowWidget.generated.h"

class UInventoryComponent;
class UEquipmentComponent;
class UEquipmentSlotWidget;
class UInventoryItem;

UCLASS()
class NON_API UCharacterWindowWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // UIManager가 호출할 주입 함수
    UFUNCTION(BlueprintCallable, Category = "CharacterWindow")
    void InitCharacterUI(UInventoryComponent* InInv, UEquipmentComponent* InEq);

    UPROPERTY() bool bEquipDelegatesBound = false;

    UFUNCTION(BlueprintCallable, Category = "CharacterWindow")
    void RefreshAllSlots();
    // 장착 상태를 슬롯들에 다시 반영
    void PropagateEquipmentToSlots();

    void ReapplyTwoHandMirrors();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UFUNCTION() void HandleEquipped(EEquipmentSlot EquipSlot, UInventoryItem* Item);

    UFUNCTION() void HandleUnequipped(EEquipmentSlot EquipSlot);

    UPROPERTY() bool bInitOnce = false;

    //  UI에 배치된 두 슬롯 포인터
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEquipmentSlotWidget> WeaponMainSlot;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEquipmentSlotWidget> WeaponSubSlot;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInventoryComponent> OwnerInventory = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UEquipmentComponent> OwnerEquipment = nullptr;

private:
};
