#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NonMerchantWindowWidget.generated.h"

class UScrollBox;
class UButton;
class UNonShopItemSlotWidget;
class ANPCCharacter;
class UInventoryComponent;
class UEquipmentComponent;

UCLASS()
class NON_API UNonMerchantWindowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void InitializeMerchantWindow(ANPCCharacter* InMerchantNPC, UInventoryComponent* InPlayerInventory, UEquipmentComponent* InPlayerEquipment);

protected:
    virtual void NativeConstruct() override;

    UFUNCTION()
    void OnCloseButtonClicked();

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UScrollBox> ScrollBox_MerchantItems;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Close;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UUserWidget> WBP_PlayerInventory;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
    TSubclassOf<UNonShopItemSlotWidget> ShopSlotWidgetClass;

private:
    UPROPERTY()
    TWeakObjectPtr<ANPCCharacter> TargetMerchant;

    UPROPERTY()
    TWeakObjectPtr<UInventoryComponent> PlayerInventory;
};
