#include "UI/Inventory/InventorySlotWidget.h"
#include "UI/Inventory/ItemToolTipWidget.h"
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
                    CooldownMID->SetScalarParameterValue(TEXT("Fill"), 0.f);
                }
                else
                {

                }
            }
            else
            {

            }
        }
        else
        {

        }
    }
    else
    {

    }

    RefreshFromInventory();
    if (ImgIcon)           ImgIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (TxtCount)          TxtCount->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (TxtCooldown)       TxtCooldown->SetVisibility(ESlateVisibility::Collapsed);
    if (ImgCooldownRadial) ImgCooldownRadial->SetVisibility(ESlateVisibility::Hidden);

    // [New] 언리얼 5.7 공식 안전 장치: 툴팁 호출용 델리게이트에 동적 C++ 헬퍼 함수 바인딩!
    ToolTipWidgetDelegate.BindDynamic(this, &UInventorySlotWidget::GetCustomToolTipWidget);
}



void UInventorySlotWidget::NativeDestruct()
{
    if (OwnerInventory)
    {
        OwnerInventory->OnSlotUpdated.RemoveAll(this);
        OwnerInventory->OnInventoryRefreshed.RemoveAll(this);
        OwnerInventory->OnCooldownStarted.RemoveAll(this);
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
            // 1. 아이콘 이미지 리소스 주입
            if (UImage* Img = Cast<UImage>(VW->GetWidgetFromName(TEXT("IconImage"))))
            {
                FSlateBrush Brush = Img->GetBrush();
                Brush.SetResourceObject(Item->GetIcon());
                Brush.ImageSize = FVector2D(IconSize, IconSize); // 고정
                Brush.DrawAs = ESlateBrushDrawType::Image;
                Img->SetBrush(Brush);
                Img->SetVisibility(ESlateVisibility::HitTestInvisible);
            }

            // 2. [New UI UX] 드래그 비주얼 위젯에도 등급 테두리(BorderIconFrame)가 존재하면 실시간 등급 전용 컬러 포인트 주입
            if (UBorder* Frame = Cast<UBorder>(VW->GetWidgetFromName(TEXT("BorderIconFrame"))))
            {
                FLinearColor RarityColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
                if (RarityColors.Contains(Item->CachedRow.Rarity))
                {
                    RarityColor = RarityColors[Item->CachedRow.Rarity];
                }
                Frame->SetBrushColor(RarityColor);

                // 2.5. [New UI UX] 드래그 뒷배경 보더(BorderBackground)가 존재하면 등급 전용 컬러의 은은한 반투명 아우라 실시간 연동 주입
                if (UBorder* Bg = Cast<UBorder>(VW->GetWidgetFromName(TEXT("BorderBackground"))))
                {
                    FLinearColor BgColor = RarityColor;
                    BgColor.A = Bg->GetBrushColor().A; // 디자이너가 설정한 알파값 보존
                    Bg->SetBrushColor(BgColor);
                }
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
        OwnerInventory->OnCooldownStarted.AddDynamic(this, &UInventorySlotWidget::OnInventoryCooldownStarted);
    }

    // [이중 안전장치] 인스턴스가 런타임에 조립될 때도 델리게이트가 절대 풀리지 않도록 재바인딩해 줍니다.
    ToolTipWidgetDelegate.BindDynamic(this, &UInventorySlotWidget::GetCustomToolTipWidget);

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
    // 수동 호출용 (호환성 유지)
    if (Remaining > 0.f && Duration > 0.f)
    {
        // 내부 타이머 시스템으로 이관
        // 현재 시간 역산
        if (UWorld* World = GetWorld())
        {
            float Now = World->GetTimeSeconds();
            StartCooldown(Duration, Now + Remaining);
        }
    }
    else
    {
        ClearCooldownUI();
    }
}

void UInventorySlotWidget::StartCooldown(float Duration, float EndTime)
{
    if (Duration <= 0.f) return;

    CooldownTotal = Duration;
    CooldownEndTime = EndTime;
    bCooldownActive = true;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(CooldownTimerHandle, this, &UInventorySlotWidget::UpdateCooldownTick, 0.05f, true);
    }

    if (ImgCooldownRadial) ImgCooldownRadial->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UpdateCooldownTick();
}

void UInventorySlotWidget::ClearCooldownUI()
{
    bCooldownActive = false;
    CooldownEndTime = 0.f;
    CooldownTotal = 0.f;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CooldownTimerHandle);
    }

    if (ImgCooldownRadial) ImgCooldownRadial->SetVisibility(ESlateVisibility::Hidden);
    
    if (TxtCooldown)
    {
        TxtCooldown->SetText(FText::GetEmpty());
        TxtCooldown->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (CooldownMID)
    {
        CooldownMID->SetScalarParameterValue(TEXT("Fill"), 0.f);
    }
}

void UInventorySlotWidget::UpdateCooldownTick()
{
    if (!bCooldownActive)
    {
        ClearCooldownUI();
        return;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    const float Now = World->GetTimeSeconds();
    const float Remaining = CooldownEndTime - Now;

    if (Remaining <= 0.f)
    {
        ClearCooldownUI();
        return;
    }

    // Update Visual
    if (ImgCooldownRadial && CooldownTotal > 0.f)
    {
        const float Ratio = FMath::Clamp(Remaining / CooldownTotal, 0.f, 1.f);
        if (CooldownMID)
        {
            CooldownMID->SetScalarParameterValue(TEXT("Fill"), Ratio);
        }
        else
        {
            // Fallback: Opacity
            FLinearColor Tint = ImgCooldownRadial->GetColorAndOpacity();
            Tint.A = Ratio * 0.7f; // 너무 진하지 않게
            ImgCooldownRadial->SetColorAndOpacity(Tint);
        }
    }
    
    // Update Text
    if (TxtCooldown)
    {
        const int32 Seconds = FMath::CeilToInt(Remaining);
        // 0초가 되기 직전까지 1초로 표시
        if (Seconds > 0)
        {
            TxtCooldown->SetText(FText::FromString(FString::Printf(TEXT("%ds"), Seconds)));
            TxtCooldown->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            // 거의 끝남
            TxtCooldown->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void UInventorySlotWidget::OnInventoryCooldownStarted(FName GroupId, float Duration, float EndTime)
{
    if (!Item) return;
    if (Item->CachedRow.ItemType == EItemType::Consumable &&
        Item->CachedRow.Consumable.CooldownGroupId == GroupId)
    {
        StartCooldown(Duration, EndTime);
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

    // Cooldown Check
    if (Item && OwnerInventory)
    {
        if (Item->CachedRow.ItemType == EItemType::Consumable)
        {
            const FName GID = Item->CachedRow.Consumable.CooldownGroupId;
            float Rem = 0.f, Tot = 0.f;
            if (OwnerInventory->GetCooldownRemaining(GID, Rem, Tot))
            {
                if (UWorld* World = GetWorld())
                {
                    StartCooldown(Tot, World->GetTimeSeconds() + Rem);
                }
            }
            else
            {
                 // 쿨타임 없으면 UI 끄기 (이미 켜져있을 수도 있으니)
                 if(!bCooldownActive) ClearCooldownUI();
                 // 주의: bCooldownActive가 true인데 GetCooldownRemaining이 false면 끝난 거임 -> UpdateCooldownTick에서 처리됨.
                 // 하지만 즉시 반영을 위해
                 if(bCooldownActive && Rem <= 0.f) ClearCooldownUI();
            }
        }
        else
        {
            ClearCooldownUI();
        }
    }
    else
    {
        ClearCooldownUI();
    }

    // ── [New] 등급별 배경 색상 및 외곽 테두리선 틴트 업데이트 ───────────────────
    if (BorderSlot || BorderOutline)
    {
        FLinearColor TargetColor = DefaultSlotColor;

        if (Item)
        {
            EItemRarity Rarity = Item->CachedRow.Rarity;
            if (RarityColors.Contains(Rarity))
            {
                TargetColor = RarityColors[Rarity];
            }
        }

        // 배경 그라데이션 슬롯 색상 적용
        if (BorderSlot)
        {
            BorderSlot->SetBrushColor(TargetColor);
        }

        // 외곽 테두리선(Outline) 색상 적용 (세트로 아름답게 물들입니다!)
        if (BorderOutline)
        {
            BorderOutline->SetBrushColor(TargetColor);
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

void UInventorySlotWidget::SetFilterState(bool bIsFilteredOut)
{
    if (bIsFilteredOut)
    {
        // 필터에서 탈락한 경우: 슬롯 전체의 불투명도를 크게 낮춰 은은하게 가라앉힙니다. (회색조 비활성화 연출)
        SetRenderOpacity(0.2f);
        
        // 클릭이나 드래그 등 불필요한 마우스 감지를 일시 차단하여 활성화된 슬롯에만 집중하게 제어합니다.
        SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        // 필터 해제 시 원래대로 선명함과 상호작용 활성화 복귀
        SetRenderOpacity(1.0f);
        SetVisibility(ESlateVisibility::Visible);
    }
}

UWidget* UInventorySlotWidget::GetCustomToolTipWidget()
{
    // 슬롯에 아이템이 없으면 툴팁을 띄우지 않습니다.
    if (!Item) return nullptr;

    // 에디터에서 툴팁 위젯 클래스를 미지정했으면 미출력
    if (!ToolTipWidgetClass) return nullptr;

    // 툴팁 위젯 인스턴스를 엔진 시스템으로 동적 생성
    UItemToolTipWidget* ToolTip = CreateWidget<UItemToolTipWidget>(this, ToolTipWidgetClass);
    if (ToolTip)
    {
        // 툴팁 내부에 현재 아이템의 상세 데이터를 주입하고 가공합니다.
        ToolTip->SetItemData(Item);
        return ToolTip;
    }

    return nullptr;
}
