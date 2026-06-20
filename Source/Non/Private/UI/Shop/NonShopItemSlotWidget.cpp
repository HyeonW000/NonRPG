#include "UI/Shop/NonShopItemSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Inventory/InventoryComponent.h"
#include "Data/ItemStructs.h"
#include "Engine/DataTable.h"

void UNonShopItemSlotWidget::InitializeSlot(FName InItemId, UInventoryComponent* InPlayerInventory)
{
    ItemId = InItemId;
    PlayerInventory = InPlayerInventory;

    if (!PlayerInventory.IsValid() || !PlayerInventory->ItemDataTable) return;

    const FItemRow* ItemRow = PlayerInventory->ItemDataTable->FindRow<FItemRow>(ItemId, TEXT("ShopSlotLoad"));
    if (!ItemRow) return;

    if (TextBlock_ItemName)
    {
        TextBlock_ItemName->SetText(ItemRow->Name);
    }

    if (TextBlock_ItemPrice)
    {
        FString PriceStr = FString::Printf(TEXT("%s Gold"), *FText::AsNumber(ItemRow->BuyPrice).ToString());
        TextBlock_ItemPrice->SetText(FText::FromString(PriceStr));
    }

    if (Image_Icon && !ItemRow->Icon.IsNull())
    {
        UTexture2D* LoadedIcon = ItemRow->Icon.LoadSynchronous();
        if (LoadedIcon)
        {
            Image_Icon->SetBrushFromTexture(LoadedIcon);
        }
    }
}

void UNonShopItemSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Button_Buy)
    {
        Button_Buy->OnClicked.AddDynamic(this, &UNonShopItemSlotWidget::OnBuyButtonClicked);
    }
}

void UNonShopItemSlotWidget::OnBuyButtonClicked()
{
    if (PlayerInventory.IsValid() && !ItemId.IsNone())
    {
        PlayerInventory->ServerBuyItem(ItemId, 1);
    }
}
