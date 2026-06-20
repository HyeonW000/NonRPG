#include "UI/Shop/NonMerchantWindowWidget.h"
#include "Components/ScrollBox.h"
// TextBlock header removed
#include "Components/Button.h"
#include "UI/Inventory/InventoryWidget.h"
#include "UI/Shop/NonShopItemSlotWidget.h"
#include "Character/NPCCharacter.h"
#include "Inventory/InventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetTree.h"
#include "Core/NonUIManagerComponent.h"

void UNonMerchantWindowWidget::InitializeMerchantWindow(ANPCCharacter* InMerchantNPC, UInventoryComponent* InPlayerInventory, UEquipmentComponent* InPlayerEquipment)
{
    TargetMerchant = InMerchantNPC;
    PlayerInventory = InPlayerInventory;

    if (!PlayerInventory.IsValid()) return;

    // 1. 우측 가방 위젯 동적 형변환 및 자식 트리 딥서치 연동
    if (WBP_PlayerInventory)
    {
        UInventoryWidget* InvWidget = Cast<UInventoryWidget>(WBP_PlayerInventory.Get());

        if (!InvWidget)
        {
            if (UWidgetTree* WT = WBP_PlayerInventory->WidgetTree)
            {
                TArray<UWidget*> AllWidgets;
                WT->GetAllWidgets(AllWidgets);
                for (UWidget* W : AllWidgets)
                {
                    if (UInventoryWidget* FoundWidget = Cast<UInventoryWidget>(W))
                    {
                        InvWidget = FoundWidget;
                        break;
                    }
                }
            }
        }

        if (InvWidget)
        {
            InvWidget->InitInventoryUI(InPlayerInventory, InPlayerEquipment);
        }
    }



    // 2. 좌측 상인 판매 물목 스크롤 슬롯 빌드
    if (ScrollBox_MerchantItems)
    {
        ScrollBox_MerchantItems->ClearChildren();
    }

    if (InMerchantNPC && ShopSlotWidgetClass)
    {
        for (const FName& ItemId : InMerchantNPC->ShopItems)
        {
            if (ItemId.IsNone()) continue;

            UNonShopItemSlotWidget* NewSlot = CreateWidget<UNonShopItemSlotWidget>(this, ShopSlotWidgetClass);
            if (NewSlot)
            {
                NewSlot->InitializeSlot(ItemId, InPlayerInventory);
                ScrollBox_MerchantItems->AddChild(NewSlot);
            }
        }
    }
}

void UNonMerchantWindowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Button_Close)
    {
        Button_Close->OnClicked.AddDynamic(this, &UNonMerchantWindowWidget::OnCloseButtonClicked);
    }
}

void UNonMerchantWindowWidget::OnCloseButtonClicked()
{
    if (APawn* OwningPawn = GetOwningPlayerPawn())
    {
        if (UNonUIManagerComponent* UIMgr = OwningPawn->FindComponentByClass<UNonUIManagerComponent>())
        {
            UIMgr->CloseMerchantShop();
            return;
        }
    }
    RemoveFromParent();
}


