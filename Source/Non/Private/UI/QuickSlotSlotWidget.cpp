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

void UQuickSlotSlotWidget::SetManager(UQuickSlotManager* InManager)
{
    Manager = InManager;
}

static TSharedPtr<SViewport> GetGameViewportSViewport(UWorld* World)
{
    if (!World) return nullptr;
    if (UGameViewportClient* GVC = World->GetGameViewport())
    {
        return GVC->GetGameViewportWidget();
    }
    return nullptr;
}

void UQuickSlotSlotWidget::UpdateVisual(UInventoryItem* Item)
{
    // 총 수량(같은 ItemId 전체)
    int32 Total = 0;
    bool bAssigned = true; // 슬롯은 배정되어 있다고 가정(고스트 유지용)
    if (Manager.IsValid() && QuickIndex >= 0)
    {
        Total = Manager->GetTotalCountForSlot(QuickIndex);
#if 1
        bAssigned = Manager->IsSlotAssigned(QuickIndex); // 안전망: 실제 배정 여부
#endif
    }
    else if (Item)
    {
        Total = Item->GetStackCount(); // 폴백
    }

    // 아이콘
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
                // 배정 유지: 마지막 아이콘 그대로(수량 0이면 고스트가 됨)
                IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
            }
            else
            {
                // 완전 해제
                CachedIcon = nullptr;
                IconImage->SetBrush(FSlateBrush());
                IconImage->SetVisibility(ESlateVisibility::Collapsed);
            }
        }

        // 고스트 처리(수량 0이면 반투명)
        if (IconImage->GetVisibility() != ESlateVisibility::Collapsed)
        {
            const float Opacity = (Total > 0) ? 1.0f : 0.35f;
            IconImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, Opacity));
        }
    }

    // 수량 텍스트 (장비는 애초에 못 올라오므로 단순 규칙만 유지)
   if (CountText)
{
    if (Total >= 1) // 1도 보이게
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

        if (TSharedPtr<SViewport> VP = GetGameViewportSViewport(GetWorld()))
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

        // --- 아이콘 텍스처 구하기 ---
        UTexture2D* Icon = Item->GetIcon();

        // --- 1) DragVisualClass가 있으면 사용 (단, DPI/가시성 보정) ---
        if (DragVisualClass)
        {
            if (UUserWidget* Visual = CreateWidget<UUserWidget>(GetWorld(), DragVisualClass))
            {
                // 드래그 비주얼 루트는 히트테스트 없이 (포인터 충돌/패딩 영향 최소화)
                Visual->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

                // BP 안의 이미지가 "IconImage" 라는 이름이라고 가정
                if (UImage* Img = Cast<UImage>(Visual->GetWidgetFromName(TEXT("IconImage"))))
                {
                    if (Icon)
                    {
                        Img->SetBrushFromTexture(Icon);

                        // DPI 보정된 사이즈 지정
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
                Op->Pivot = EDragPivot::MouseDown;      // 눌렀던 지점과 커서 일치
                Op->Offset = FVector2D::ZeroVector;      // 추가 오프셋 없음
            }
        }
        else
        {
            // --- 2) Fallback: BP 없이 "타이트한" UImage 하나로 드래그 비주얼 구성 ---
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

    // 1) 퀵슬롯에서 온 드래그
    if (Op->bFromQuickSlot && Op->SourceQuickIndex != INDEX_NONE)
    {
        if (Op->SourceQuickIndex == QuickIndex) return true; // 자기 자신

        UInventoryItem* SrcItem = Manager->ResolveItem(Op->SourceQuickIndex);
        UInventoryItem* DstItem = Manager->ResolveItem(QuickIndex);

        // (a) 목적지가 비었으면 "이동"
        if (!DstItem)
        {
            return Manager->MoveSlot(Op->SourceQuickIndex, QuickIndex);
        }

        // (b) 두 슬롯 아이템이 같다면(아이디 동일) → 소스만 비움(유일성)
        if (SrcItem && DstItem && SrcItem->ItemId == DstItem->ItemId)
        {
            Manager->ClearSlot(Op->SourceQuickIndex);
            return true;
        }

        // (c) 서로 다른 아이템이면 기존처럼 스왑
        return Manager->SwapSlots(QuickIndex, Op->SourceQuickIndex);
    }

    // 2) 인벤토리에서 온 드래그 → 기존 할당
    if (Op->SourceInventory && Op->SourceIndex != INDEX_NONE)
    {
        // AssignFromInventory 내부에서 같은 아이템이 다른 슬롯에 있으면 자동으로 지움(유일성)
        return Manager->AssignFromInventory(QuickIndex, Op->SourceInventory, Op->SourceIndex);
    }

    return false;
}
