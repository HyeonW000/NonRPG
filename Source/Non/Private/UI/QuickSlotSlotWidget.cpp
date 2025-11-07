#include "UI/QuickSlotSlotWidget.h"
#include "UI/QuickSlotManager.h"
#include "Inventory/ItemDragDropOperation.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryItem.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Widgets/SViewport.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"

// 공용 유틸
#include "UI/UIViewportUtils.h"

void UQuickSlotSlotWidget::SetManager(UQuickSlotManager* InManager)
{
    Manager = InManager;
}

void UQuickSlotSlotWidget::UpdateVisual(UInventoryItem* Item)
{
    int32 Total = 0;
    bool bAssigned = true;
    if (Manager.IsValid() && QuickIndex >= 0)
    {
        Total = Manager->GetTotalCountForSlot(QuickIndex);
        bAssigned = Manager->IsSlotAssigned(QuickIndex);
    }
    else if (Item)
    {
        Total = Item->GetStackCount();
    }

    if (IconImage)
    {
        if (Item && Item->GetIcon())
        {
            UTexture2D* NewTex = Item->GetIcon();
            IconImage->SetBrushFromTexture(NewTex);
            CachedIcon = NewTex;
            IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            if (bAssigned && (CachedIcon != nullptr || IconImage->GetBrush().GetResourceObject() != nullptr))
            {
                IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
            }
            else
            {
                CachedIcon = nullptr;
                IconImage->SetBrush(FSlateBrush());
                IconImage->SetVisibility(ESlateVisibility::Collapsed);
            }
        }

        if (IconImage->GetVisibility() != ESlateVisibility::Collapsed)
        {
            const float Opacity = (Total > 0) ? 1.0f : 0.35f;
            IconImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, Opacity));
        }
    }

    if (CountText)
    {
        if (Total >= 1)
        {
            CountText->SetText(FText::AsNumber(Total));
            CountText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            CountText->SetText(FText::GetEmpty());
            CountText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    UpdateVisualBP(Item);
}

void UQuickSlotSlotWidget::Refresh()
{
    if (!Manager.IsValid() || QuickIndex < 0)
    {
        UpdateVisual(nullptr);
        return;
    }

    UInventoryItem* Item = Manager->ResolveItem(QuickIndex);

    if (Item && Item->GetStackCount() <= 0)
    {
        Item = nullptr;
    }

    UpdateVisual(Item);
}

FReply UQuickSlotSlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    if (E.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (APlayerController* PC = GetOwningPlayer())
        {
            FInputModeGameAndUI Mode;
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Mode.SetHideCursorDuringCapture(false);
            Mode.SetWidgetToFocus(nullptr);
            PC->SetInputMode(Mode);
        }

        FEventReply ER = UWidgetBlueprintLibrary::DetectDragIfPressed(E, this, EKeys::LeftMouseButton);
        FReply Reply = ER.NativeReply;

        if (TSharedPtr<SViewport> VP = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
        {
            Reply = Reply.SetUserFocus(StaticCastSharedRef<SWidget>(VP.ToSharedRef()), EFocusCause::SetDirectly);
            FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
        }
        return Reply;
    }
    return Super::NativeOnPreviewMouseButtonDown(G, E);
}

void UQuickSlotSlotWidget::NativeOnDragDetected(const FGeometry& G, const FPointerEvent& E, UDragDropOperation*& OutOperation)
{
    OutOperation = nullptr;
    if (!Manager.IsValid() || QuickIndex < 0) return;

    if (UInventoryItem* Item = Manager->ResolveItem(QuickIndex))
    {
        UItemDragDropOperation* Op = NewObject<UItemDragDropOperation>(this);
        Op->bFromQuickSlot = true;
        Op->SourceQuickIndex = QuickIndex;
        Op->SourceQuickManager = Manager.Get();
        Op->Item = Item;

        UTexture2D* Icon = Item->GetIcon();

        if (DragVisualClass)
        {
            if (UUserWidget* Visual = CreateWidget<UUserWidget>(GetWorld(), DragVisualClass))
            {
                Visual->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

                if (UImage* Img = Cast<UImage>(Visual->GetWidgetFromName(TEXT("IconImage"))))
                {
                    if (Icon)
                    {
                        Img->SetBrushFromTexture(Icon);

                        const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
                        const FVector2D TexSize(Icon->GetSizeX(), Icon->GetSizeY());
                        if (Scale > 0.f && TexSize.X > 0.f && TexSize.Y > 0.f)
                        {
                            Visual->SetDesiredSizeInViewport(TexSize / Scale);
                        }
                    }
                    else
                    {
                        Img->SetBrush(FSlateBrush());
                    }
                }

                Op->DefaultDragVisual = Visual;
                Op->Pivot = EDragPivot::MouseDown;
                Op->Offset = FVector2D::ZeroVector;
            }
        }
        else
        {
            if (Icon)
            {
                UImage* Img = NewObject<UImage>(this);
                Img->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
                FSlateBrush Brush;
                Brush.SetResourceObject(Icon);
                Brush.ImageSize = FVector2D(Icon->GetSizeX(), Icon->GetSizeY());
                Img->SetBrush(Brush);

                Op->DefaultDragVisual = Img;
                Op->Pivot = EDragPivot::MouseDown;
                Op->Offset = FVector2D::ZeroVector;
            }
        }

        OutOperation = Op;
    }
}

bool UQuickSlotSlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    return Cast<UItemDragDropOperation>(InOperation) != nullptr;
}

bool UQuickSlotSlotWidget::NativeOnDrop(const FGeometry& G, const FDragDropEvent& E, UDragDropOperation* InOp)
{
    UItemDragDropOperation* Op = Cast<UItemDragDropOperation>(InOp);
    if (!Op || !Manager.IsValid()) return false;

    if (Op->bFromQuickSlot && Op->SourceQuickIndex != INDEX_NONE)
    {
        if (Op->SourceQuickIndex == QuickIndex) return true;

        UInventoryItem* SrcItem = Manager->ResolveItem(Op->SourceQuickIndex);
        UInventoryItem* DstItem = Manager->ResolveItem(QuickIndex);

        if (!DstItem)
        {
            return Manager->MoveSlot(Op->SourceQuickIndex, QuickIndex);
        }

        if (SrcItem && DstItem && SrcItem->ItemId == DstItem->ItemId)
        {
            Manager->ClearSlot(Op->SourceQuickIndex);
            return true;
        }

        return Manager->SwapSlots(QuickIndex, Op->SourceQuickIndex);
    }

    if (Op->SourceInventory && Op->SourceIndex != INDEX_NONE)
    {
        return Manager->AssignFromInventory(QuickIndex, Op->SourceInventory, Op->SourceIndex);
    }

    return false;
}
