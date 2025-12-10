#include "UI/Inventory/InventorySlotWidget.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryItem.h"
#include "Inventory/ItemDragDropOperation.h"
#include "Inventory/ItemUseLibrary.h"
#include "Equipment/EquipmentComponent.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "InputCoreTypes.h"

#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWidget.h"

// 공용 유틸
#include "UI/UIViewportUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogInvDrag, Log, All);

UInventorySlotWidget::UInventorySlotWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(false);
}

void UInventorySlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (ImgCooldownRadial)
    {
        ImgCooldownRadial->SetVisibility(ESlateVisibility::Hidden);
        if (UObject* ResObj = ImgCooldownRadial->GetBrush().GetResourceObject())
        {
            if (UMaterialInterface* MI = Cast<UMaterialInterface>(ResObj))
            {
                CooldownMID = UMaterialInstanceDynamic::Create(MI, this);
                if (CooldownMID)
                {
                    FSlateBrush B = ImgCooldownRadial->GetBrush();
                    B.SetResourceObject(CooldownMID);
                    ImgCooldownRadial->SetBrush(B);
                    CooldownMID->SetScalarParameterValue(TEXT("Percent"), 0.f);
                }
            }
        }
    }

    RefreshFromInventory();
    if (ImgIcon)           ImgIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (TxtCount)          TxtCount->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (ImgCooldownRadial) ImgCooldownRadial->SetVisibility(ESlateVisibility::Hidden);
}

void UInventorySlotWidget::NativeDestruct()
{
    if (OwnerInventory)
    {
        OwnerInventory->OnSlotUpdated.RemoveAll(this);
        OwnerInventory->OnInventoryRefreshed.RemoveAll(this);
    }
    Super::NativeDestruct();
}

void UInventorySlotWidget::NativeTick(const FGeometry& G, float InDeltaTime)
{
    Super::NativeTick(G, InDeltaTime);

    if (!FSlateApplication::IsInitialized()) return;

    if (bDraggingItemActive || FSlateApplication::Get().IsDragDropping())
    {
        if (TSharedPtr<SViewport> VP = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
        {
            const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
            FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedRef<SWidget>(VP.ToSharedRef()), EFocusCause::SetDirectly);
            FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
        }
    }
}

FReply UInventorySlotWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
    if (TSharedPtr<SViewport> VP = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedRef<SWidget>(VP.ToSharedRef()), EFocusCause::SetDirectly);
        FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
    }
    return FReply::Handled();
}

FReply UInventorySlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    if (E.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        const double Now = FSlateApplication::IsInitialized()
            ? FSlateApplication::Get().GetCurrentTime()
            : FPlatformTime::Seconds();

        const FVector2D CurrSS = E.GetScreenSpacePosition();
        const float ViewScale = UWidgetLayoutLibrary::GetViewportScale(this);
        const float DistThresh = DragSlopPixels * FMath::Max(ViewScale, 0.01f);

        const bool bTimeOk = (Now - LastClickTime) <= DoubleClickSeconds;
        const bool bDistOk = (CurrSS - LastClickPosSS).Size() <= DistThresh;
        
        // --- Double Click Logic (Use Item) ---
        if (bTimeOk && bDistOk)
        {
            KeepGameInputFocus(); // Already does SetFocus to Viewport
            TryUseThisItem();

            LastClickTime = 0.0;
            bLeftPressed = false;
            bDragArmed = false;
            return FReply::Handled();
        }

        LastClickTime = Now;
        LastClickPosSS = CurrSS;
        MouseDownStartSS = CurrSS;
        bLeftPressed = true;
        bDragArmed = true;

        // Keep Focus (InputMode + Focus Viewport)
        KeepGameInputFocus();

        // --- Drag Detection Logic (Updated to match QuickSlot) ---
        // Instead of manual CaptureMouse, use DetectDragIfPressed or similar if possible.
        // However, InventorySlot uses manual dragging logic in NativeOnMouseMove too?
        // Let's stick to UWidgetBlueprintLibrary::DetectDragIfPressed for consistency if feasible, 
        // BUT InventorySlot has existing bDragArmed logic.
        // For minimal breakage while fixing the WASD issue:
        // Main fix: Ensure the REPLY explicitly sets UserFocus/KeyboardFocus to Viewport even if capturing.
        
        FReply Reply = FReply::Handled()
            .DetectDrag(TakeWidget(), EKeys::LeftMouseButton)
            .CaptureMouse(TakeWidget());

        if (TSharedPtr<SViewport> VP = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
        {
            Reply = Reply.SetUserFocus(StaticCastSharedRef<SWidget>(VP.ToSharedRef()), EFocusCause::SetDirectly);
            FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
        }

        return Reply;
    }
    return Super::NativeOnPreviewMouseButtonDown(G, E);
}

FReply UInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    if (E.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        const double Now = FSlateApplication::IsInitialized()
            ? FSlateApplication::Get().GetCurrentTime()
            : FPlatformTime::Seconds();
        LastClickTime = Now;
        LastClickPosSS = E.GetScreenSpacePosition();
        MouseDownStartSS = LastClickPosSS;
        bLeftPressed = true;
        bDragArmed = true;

        if (APlayerController* PC = GetOwningPlayer())
        {
            FInputModeGameAndUI Mode;
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Mode.SetHideCursorDuringCapture(false);
            Mode.SetWidgetToFocus(nullptr);
            PC->SetInputMode(Mode);
        }

        FEventReply Ev = UWidgetBlueprintLibrary::DetectDragIfPressed(E, this, EKeys::LeftMouseButton);
        return Ev.NativeReply;
    }

    return Super::NativeOnMouseButtonDown(G, E);
}

FReply UInventorySlotWidget::NativeOnMouseButtonUp(const FGeometry& G, const FPointerEvent& E)
{
    if (E.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragArmed = false;
        bLeftPressed = false;

        KeepGameInputFocus();

        FReply Reply = FReply::Handled().ReleaseMouseCapture();
        return Reply;
    }
    return Super::NativeOnMouseButtonUp(G, E);
}

FReply UInventorySlotWidget::NativeOnMouseMove(const FGeometry& G, const FPointerEvent& E)
{
    if (bDragArmed && E.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        const float ViewScale = UWidgetLayoutLibrary::GetViewportScale(this);
        const float Slop = DragSlopPixels * FMath::Max(ViewScale, 0.01f);
        const float Dist = (E.GetScreenSpacePosition() - MouseDownStartSS).Size();
    }

    return Super::NativeOnMouseMove(G, E);
}

void UInventorySlotWidget::NativeOnDragDetected(
    const FGeometry& G, const FPointerEvent& E, UDragDropOperation*& OutOp)
{
    bDragArmed = false;
    bDraggingItemActive = true;

    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetWidgetToFocus(nullptr);
        PC->SetInputMode(Mode);
    }

    if (TSharedPtr<SViewport> VP = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedRef<SWidget>(VP.ToSharedRef()), EFocusCause::SetDirectly);
        FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
    }

    if (!Item || !OwnerInventory)
    {
        bDraggingItemActive = false;
        bDragArmed = false;
        OutOp = nullptr;
        return;
    }

    UItemDragDropOperation* Op = NewObject<UItemDragDropOperation>(this);
    Op->Item = Item;
    Op->SourceInventory = OwnerInventory;
    Op->SourceIndex = SlotIndex;

    const float IconSize = 64.f;
    UWidget* Visual = nullptr;

    if (DragVisualClass)
    {
        if (UUserWidget* VW = CreateWidget<UUserWidget>(GetWorld(), DragVisualClass))
        {
            if (UImage* Img = Cast<UImage>(VW->GetWidgetFromName(TEXT("IconImage"))))
            {
                FSlateBrush Brush = Img->GetBrush();
                Brush.SetResourceObject(Item->GetIcon());
                Brush.ImageSize = FVector2D(IconSize, IconSize); // 고정
                Brush.DrawAs = ESlateBrushDrawType::Image;
                Img->SetBrush(Brush);
                Img->SetVisibility(ESlateVisibility::HitTestInvisible);
            }
            VW->SetDesiredSizeInViewport(FVector2D(IconSize, IconSize));
            Visual = VW;

            Op->DefaultDragVisual = Visual;
            Op->Pivot = EDragPivot::MouseDown;
            Op->Offset = FVector2D::ZeroVector;
        }
    }
    OutOp = Op;
}

void UInventorySlotWidget::NativeOnDragCancelled(const FDragDropEvent& DragDropEvent, UDragDropOperation* InOp)
{
    bDraggingItemActive = false;

    if (TSharedPtr<SViewport> VP = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedRef<SWidget>(VP.ToSharedRef()), EFocusCause::SetDirectly);
        FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
    }
    Super::NativeOnDragCancelled(DragDropEvent, InOp);
}

bool UInventorySlotWidget::NativeOnDragOver(const FGeometry& G, const FDragDropEvent& DragDropEvent, UDragDropOperation* InOp)
{
    return Cast<UItemDragDropOperation>(InOp) != nullptr;
}

bool UInventorySlotWidget::NativeOnDrop(const FGeometry& G, const FDragDropEvent& DragDropEvent, UDragDropOperation* InOp)
{
    UItemDragDropOperation* Op = Cast<UItemDragDropOperation>(InOp);
    if (!Op || !OwnerInventory || SlotIndex == INDEX_NONE)
    {
        bDraggingItemActive = false;
        bDragArmed = false;
        return false;
    }

    bool bResult = false;

    if (Op->bFromEquipment && Op->FromEquipSlot != EEquipmentSlot::None)
    {
        UEquipmentComponent* Eq = nullptr;
        if (AActor* InvOwner = OwnerInventory->GetOwner())
        {
            Eq = InvOwner->FindComponentByClass<UEquipmentComponent>();
        }

        if (!Eq)
        {
            bDraggingItemActive = false;
            bDragArmed = false;
            return false;
        }

        int32 OutIndex = INDEX_NONE;
        const bool bUnequipped = Eq->UnequipToInventory(Op->FromEquipSlot, OutIndex);
        if (!bUnequipped)
        {
            bDraggingItemActive = false;
            bDragArmed = false;
            return false;
        }

        if (OutIndex != SlotIndex)
        {
            bResult = OwnerInventory->Move(OutIndex, SlotIndex);
        }
        else
        {
            bResult = true;
        }

        bDraggingItemActive = false;
        bDragArmed = false;
        return bResult;
    }

    if (Op->SourceInventory == OwnerInventory)
    {
        if (Op->SourceIndex != SlotIndex)
        {
            bResult = OwnerInventory->Move(Op->SourceIndex, SlotIndex);
        }
        else
        {
            bResult = true;
        }
    }

    bDraggingItemActive = false;
    bDragArmed = false;

    if (TSharedPtr<SViewport> VP = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedRef<SWidget>(VP.ToSharedRef()), EFocusCause::SetDirectly);
        FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
    }

    return bResult;
}

FReply UInventorySlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& Geo, const FPointerEvent& E)
{
    if (E.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        KeepGameInputFocus();

        const int32 Qty = (Item ? Item->Quantity : 0);
        TryUseThisItem();
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDoubleClick(Geo, E);
}

void UInventorySlotWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (BorderSlot)
    {
        BorderSlot->OnMouseButtonDownEvent.BindUFunction(this, FName("OnBorderMouseDown"));
        BorderSlot->OnMouseButtonUpEvent.BindUFunction(this, FName("OnBorderMouseUp"));
        BorderSlot->OnMouseMoveEvent.BindUFunction(this, FName("OnBorderMouseMove"));
    }
    else
    {
    }
}

void UInventorySlotWidget::KeepGameInputFocus()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetWidgetToFocus(nullptr);
        PC->SetInputMode(Mode);
    }
    if (TSharedPtr<SViewport> VP = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedRef<SWidget>(VP.ToSharedRef()), EFocusCause::SetDirectly);
        FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
    }
}

void UInventorySlotWidget::TryUseThisItem()
{
    if (!OwnerInventory || SlotIndex == INDEX_NONE)
    {
        return;
    }

    UInventoryItem* Before = OwnerInventory->GetItemAt(SlotIndex);
    if (!Before)
    {
        return;
    }

    const int32 BeforeQty = Before->Quantity;
    const FName BeforeId = Before->ItemId;

    AActor* Context = OwnerInventory->GetOwner();
    const bool bOK = UItemUseLibrary::UseOrEquip(Context ? Context : GetOwningPlayerPawn(), OwnerInventory, SlotIndex);

    // 사용 직후 최신 상태 확인은 필요 시 작성
}

void UInventorySlotWidget::InitSlot(UInventoryComponent* InInventory, int32 InIndex)
{
    if (OwnerInventory)
    {
        OwnerInventory->OnSlotUpdated.RemoveAll(this);
        OwnerInventory->OnInventoryRefreshed.RemoveAll(this);
    }

    OwnerInventory = InInventory;
    SlotIndex = InIndex;

    if (OwnerInventory)
    {
        OwnerInventory->OnSlotUpdated.AddDynamic(this, &UInventorySlotWidget::HandleSlotUpdated);
        OwnerInventory->OnInventoryRefreshed.AddDynamic(this, &UInventorySlotWidget::HandleInventoryRefreshed);
    }

    RefreshFromInventory();
}

void UInventorySlotWidget::HandleSlotUpdated(int32 UpdatedIndex, UInventoryItem* UpdatedItem)
{
    if (UpdatedIndex == SlotIndex)
    {
        Item = UpdatedItem;
        UpdateVisual();
    }
}

void UInventorySlotWidget::HandleInventoryRefreshed()
{
    RefreshFromInventory();
}

void UInventorySlotWidget::RefreshFromInventory()
{
    if (OwnerInventory && SlotIndex != INDEX_NONE)
    {
        Item = OwnerInventory->GetItemAt(SlotIndex);
    }
    else
    {
        Item = nullptr;
    }

    UpdateVisual();
}

void UInventorySlotWidget::UpdateCooldown(float Remaining, float Duration)
{
    if (!ImgCooldownRadial) return;

    if (Remaining > 0.f && Duration > KINDA_SMALL_NUMBER)
    {
        ImgCooldownRadial->SetVisibility(ESlateVisibility::Visible);

        const float Ratio = FMath::Clamp(Remaining / Duration, 0.f, 1.f);
        if (CooldownMID)
        {
            CooldownMID->SetScalarParameterValue(TEXT("Percent"), Ratio);
        }
        else
        {
            FLinearColor Tint = ImgCooldownRadial->GetColorAndOpacity();
            Tint.A = Ratio;
            ImgCooldownRadial->SetColorAndOpacity(Tint);
        }
    }
    else
    {
        ImgCooldownRadial->SetVisibility(ESlateVisibility::Hidden);

        if (CooldownMID)
        {
            CooldownMID->SetScalarParameterValue(TEXT("Percent"), 0.f);
        }
        else
        {
            FLinearColor Tint = ImgCooldownRadial->GetColorAndOpacity();
            Tint.A = 0.f;
            ImgCooldownRadial->SetColorAndOpacity(Tint);
        }
    }
}

void UInventorySlotWidget::UpdateVisual()
{
    if (ImgIcon)
    {
        if (UTexture2D* IconTex = (Item ? Item->GetIcon() : nullptr))
        {
            FSlateBrush Brush = ImgIcon->GetBrush();
            Brush.SetResourceObject(IconTex);
            ImgIcon->SetBrush(Brush);
            ImgIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            ImgIcon->SetBrush(FSlateBrush());
            ImgIcon->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    if (TxtCount)
    {
        const bool bHasItem = (Item != nullptr);
        int32 Count = bHasItem ? Item->Quantity : 0;

        const bool bIsEquipment = bHasItem && Item->IsEquipment();
        const bool bShouldShow =
            (bHasItem && !bIsEquipment && Count >= 1) ||
            (Count > 1);

        if (bShouldShow)
        {
            const int32 Display = (!bIsEquipment && Count < 1) ? 1 : Count;
            TxtCount->SetText(FText::AsNumber(FMath::Max(1, Display)));
            TxtCount->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            TxtCount->SetText(FText::GetEmpty());
            TxtCount->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

FEventReply UInventorySlotWidget::OnBorderMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
    FReply PreviewReply = NativeOnPreviewMouseButtonDown(MyGeometry, MouseEvent);
    FReply DownReply = NativeOnMouseButtonDown(MyGeometry, MouseEvent);

    FEventReply Ev = UWidgetBlueprintLibrary::Handled();
    Ev.NativeReply = DownReply;
    return Ev;
}

FEventReply UInventorySlotWidget::OnBorderMouseUp(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
    FReply UpReply = NativeOnMouseButtonUp(MyGeometry, MouseEvent);

    FEventReply Ev = UWidgetBlueprintLibrary::Handled();
    Ev.NativeReply = UpReply;
    return Ev;
}

FEventReply UInventorySlotWidget::OnBorderMouseMove(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
    FReply MoveReply = NativeOnMouseMove(MyGeometry, MouseEvent);

    FEventReply Ev = UWidgetBlueprintLibrary::Handled();
    Ev.NativeReply = MoveReply;
    return Ev;
}
