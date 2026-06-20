#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NonShopItemSlotWidget.generated.h"

class UImage;
class UTextBlock;
class UButton;
class UInventoryComponent;

UCLASS()
class NON_API UNonShopItemSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void InitializeSlot(FName InItemId, UInventoryComponent* InPlayerInventory);

protected:
    virtual void NativeConstruct() override;

    UFUNCTION()
    void OnBuyButtonClicked();

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> Image_Icon;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TextBlock_ItemName;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TextBlock_ItemPrice;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Buy;

private:
    FName ItemId;

    UPROPERTY()
    TWeakObjectPtr<UInventoryComponent> PlayerInventory;
};
