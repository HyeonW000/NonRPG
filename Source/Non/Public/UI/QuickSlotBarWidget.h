#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuickSlotBarWidget.generated.h"

class UQuickSlotSlotWidget;
class UQuickSlotManager;
class UInventoryItem;
class ANonCharacterBase;

UCLASS()
class NON_API UQuickSlotBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 디자이너에서 넣어둔 10칸을 이름 그대로 바인딩(없어도 되는 경우가 있어 Optional)
    UPROPERTY(meta = (BindWidgetOptional)) UQuickSlotSlotWidget* Slot_0;
    UPROPERTY(meta = (BindWidgetOptional)) UQuickSlotSlotWidget* Slot_1;
    UPROPERTY(meta = (BindWidgetOptional)) UQuickSlotSlotWidget* Slot_2;
    UPROPERTY(meta = (BindWidgetOptional)) UQuickSlotSlotWidget* Slot_3;
    UPROPERTY(meta = (BindWidgetOptional)) UQuickSlotSlotWidget* Slot_4;
    UPROPERTY(meta = (BindWidgetOptional)) UQuickSlotSlotWidget* Slot_5;
    UPROPERTY(meta = (BindWidgetOptional)) UQuickSlotSlotWidget* Slot_6;
    UPROPERTY(meta = (BindWidgetOptional)) UQuickSlotSlotWidget* Slot_7;
    UPROPERTY(meta = (BindWidgetOptional)) UQuickSlotSlotWidget* Slot_8;
    UPROPERTY(meta = (BindWidgetOptional)) UQuickSlotSlotWidget* Slot_9;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    // 수집된 슬롯 목록(유효한 것만)
    UPROPERTY() TArray<TObjectPtr<UQuickSlotSlotWidget>> Slots;

    // 매니저 보관
    TWeakObjectPtr<UQuickSlotManager> Manager;

    FTimerHandle InitTimerHandle;
    int32 RetryCount = 0;
    static constexpr int32 MaxRetries = 100; // 5초 정도(0.05 * 100)

    void TryInit();

    bool CollectSlots();
    void AssignManagerToSlots();
    void InitialRefresh();

    UFUNCTION()
    void HandleQuickSlotChanged(int32 SlotIndex, UInventoryItem* Item);

    void BindManagerDelegate();
    void UnbindManagerDelegate();
};
