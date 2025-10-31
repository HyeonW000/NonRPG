#include "UI/InventorySlotWidget.h"
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
#include "Materials/MaterialInstanceDynamic.h"
#include "InputCoreTypes.h"

//  게임 뷰포트 SViewport 접근
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogInvDrag, Log, All);

// ----- 게임 뷰포트의 SViewport 포인터 얻기 (TSharedPtr!) -----
static TSharedPtr<SViewport> GetGameViewportSViewport(UWorld* World)
{
    if (!World) return nullptr;
    if (UGameViewportClient* GVC = World->GetGameViewport())
    {
        return GVC->GetGameViewportWidget();
    }
    return nullptr;
}

UInventorySlotWidget::UInventorySlotWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // 슬롯 자체가 키보드 포커스를 먹지 않도록
    SetIsFocusable(false);
}

void UInventorySlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // ==== (그대로 유지) 쿨다운 이미지/MID 준비, 초기 표시 갱신 등 ====
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

    // 드래그 중에는 뷰포트 포커스 유지(키보드 입력 끊김 방지)
    if (bDraggingItemActive || FSlateApplication::Get().IsDragDropping())
    {
        if (TSharedPtr<SViewport> VP = GetGameViewportSViewport(GetWorld()))
        {
            const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
            FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedPtr<SWidget>(VP), EFocusCause::SetDirectly);
            FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
        }
    }
}

FReply UInventorySlotWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
    if (TSharedPtr<SViewport> VP = GetGameViewportSViewport(GetWorld()))
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedPtr<SWidget>(VP), EFocusCause::SetDirectly);
        FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
    }
    return FReply::Handled();
}
// ===================== 마우스 입력/드래그 =====================

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

        UE_LOG(LogInvDrag, Log, TEXT("[Slot %d] PreviewMouseButtonDown (LMB? %d)"), SlotIndex,
            E.GetEffectingButton() == EKeys::LeftMouseButton);

        // 더블클릭 우선 처리
        if (bTimeOk && bDistOk)
        {
            KeepGameInputFocus();
            TryUseThisItem();

            LastClickTime = 0.0;
            bLeftPressed = false;
            bDragArmed = false;
            return FReply::Handled();
        }

        // 첫 클릭 기록 (슬롭 체크용)
        LastClickTime = Now;
        LastClickPosSS = CurrSS;
        MouseDownStartSS = CurrSS;
        bLeftPressed = true;
        bDragArmed = true;

        KeepGameInputFocus();

        // ★ 핵심: 여기서 드래그 감지+마우스 캡처를 설정
        FReply Reply = FReply::Handled()
            .DetectDrag(TakeWidget(), EKeys::LeftMouseButton)   // 슬롭 넘으면 OnDragDetected 호출됨
            .CaptureMouse(TakeWidget());                        // 밖으로 나가도 Move/Up 받게

        UE_LOG(LogInvDrag, Log, TEXT("[Slot %d] DetectDrag + CaptureMouse armed"), SlotIndex);
        return Reply;
    }
    return Super::NativeOnPreviewMouseButtonDown(G, E);
}

FReply UInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    if (E.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        UE_LOG(LogInvDrag, Log, TEXT("[Slot %d] OnMouseButtonDown: LMB"), SlotIndex);

        // 클릭 시 더블클릭용 기록 + 드래그 무장
        const double Now = FSlateApplication::IsInitialized()
            ? FSlateApplication::Get().GetCurrentTime()
            : FPlatformTime::Seconds();
        LastClickTime = Now;
        LastClickPosSS = E.GetScreenSpacePosition();
        MouseDownStartSS = LastClickPosSS;
        bLeftPressed = true;
        bDragArmed = true;

        // 게임 입력 포커스 유지
        if (APlayerController* PC = GetOwningPlayer())
        {
            FInputModeGameAndUI Mode;
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Mode.SetHideCursorDuringCapture(false);
            Mode.SetWidgetToFocus(nullptr);
            PC->SetInputMode(Mode);
        }

        // ★ 엔진 표준: 여기서 "드래그 감지 상태"를 켠다.
        //    슬롭을 넘는 순간 엔진이 자동으로 NativeOnDragDetected를 호출함.
        FEventReply Ev = UWidgetBlueprintLibrary::DetectDragIfPressed(E, this, EKeys::LeftMouseButton);
        UE_LOG(LogInvDrag, Log, TEXT("[Slot %d] DetectDragIfPressed issued"), SlotIndex);
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

        KeepGameInputFocus(); // WASD 유지

        FReply Reply = FReply::Handled().ReleaseMouseCapture();
        return Reply;
    }
    return Super::NativeOnMouseButtonUp(G, E);
}

FReply UInventorySlotWidget::NativeOnMouseMove(const FGeometry& G, const FPointerEvent& E)
{
    // 디버깅용
    if (bDragArmed && E.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        const float ViewScale = UWidgetLayoutLibrary::GetViewportScale(this);
        const float Slop = DragSlopPixels * FMath::Max(ViewScale, 0.01f);
        const float Dist = (E.GetScreenSpacePosition() - MouseDownStartSS).Size();
        UE_LOG(LogInvDrag, Verbose, TEXT("[Slot %d] MouseMove armed, dist=%.1f / slop=%.1f"),
            SlotIndex, Dist, Slop);
    }

    return Super::NativeOnMouseMove(G, E);
}

void UInventorySlotWidget::NativeOnDragDetected(const FGeometry& G, const FPointerEvent& E, UDragDropOperation*& OutOp)
{
    UE_LOG(LogInvDrag, Log, TEXT("[Slot] OnDragDetected: Slot=%d, Item=%s"), SlotIndex, *GetNameSafe(Item));
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

    if (TSharedPtr<SViewport> VP = GetGameViewportSViewport(GetWorld()))
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedPtr<SWidget>(VP), EFocusCause::SetDirectly);
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

    if (DragVisualClass)
    {
        if (UUserWidget* Visual = CreateWidget<UUserWidget>(GetWorld(), DragVisualClass))
        {
            if (UImage* Img = Cast<UImage>(Visual->GetWidgetFromName(TEXT("IconImage"))))
            {
                if (UTexture2D* Icon = Item ? Item->GetIcon() : nullptr)
                {
                    Img->SetBrushFromTexture(Icon);

                    // 경고 제거: Brush 멤버 직접 접근 대신 GetBrush().GetImageSize() 사용
                    FVector2D BrushSize = Img->GetBrush().GetImageSize();
                    if (BrushSize.IsNearlyZero())
                    {
                        // 브러시에 사이즈가 없으면 텍스처 실제 크기 사용
                        BrushSize = FVector2D(Icon->GetSizeX(), Icon->GetSizeY());
                    }

                    if (!BrushSize.IsNearlyZero())
                    {
                        const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
                        Visual->SetDesiredSizeInViewport(BrushSize / FMath::Max(Scale, 0.01f));
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

    OutOp = Op;
}

void UInventorySlotWidget::NativeOnDragCancelled(const FDragDropEvent& DragDropEvent, UDragDropOperation* InOp)
{
    bDraggingItemActive = false;

    if (TSharedPtr<SViewport> VP = GetGameViewportSViewport(GetWorld()))
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedPtr<SWidget>(VP), EFocusCause::SetDirectly);
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

    // ================== 케이스 A: 장비 → 인벤토리 슬롯 ==================
    if (Op->bFromEquipment && Op->FromEquipSlot != EEquipmentSlot::None)
    {
        // 이 슬롯 위젯이 소속된 인벤토리의 Owner에서 EquipmentComponent를 찾아온다.
        UEquipmentComponent* Eq = nullptr;
        if (AActor* InvOwner = OwnerInventory->GetOwner())
        {
            Eq = InvOwner->FindComponentByClass<UEquipmentComponent>();
        }

        if (!Eq)
        {
            UE_LOG(LogTemp, Warning, TEXT("[InvSlot] OnDrop: EquipmentComponent not found"));
            bDraggingItemActive = false;
            bDragArmed = false;
            return false;
        }

        // 1) 장착 해제 → 인벤토리로(빈칸 자동 배치)
        int32 OutIndex = INDEX_NONE;
        const bool bUnequipped = Eq->UnequipToInventory(Op->FromEquipSlot, OutIndex);
        if (!bUnequipped)
        {
            UE_LOG(LogTemp, Warning, TEXT("[InvSlot] OnDrop: UnequipToInventory failed"));
            bDraggingItemActive = false;
            bDragArmed = false;
            return false;
        }

        // 2) 우리가 드롭한 '이 슬롯'으로 자리 맞추기
        if (OutIndex != SlotIndex)
        {
            // 프로젝트에 이미 존재하는 Move(from,to) 사용.
            // Move가 false면 아이템은 OutIndex(해제된 자리)에 그대로 남습니다.
            bResult = OwnerInventory->Move(OutIndex, SlotIndex);
        }
        else
        {
            bResult = true; // 이미 원하는 자리에 들어감
        }

        // 마우스/드래그 상태 정리
        bDraggingItemActive = false;
        bDragArmed = false;

        return bResult;
    }

    // ================== 케이스 B: 같은 인벤토리 내 슬롯 이동(기존 로직) ==================
    if (Op->SourceInventory == OwnerInventory)
    {
        if (Op->SourceIndex != SlotIndex)
        {
            bResult = OwnerInventory->Move(Op->SourceIndex, SlotIndex); // 기존 코드
        }
        else
        {
            bResult = true; // 같은 칸으로 드롭한 경우
        }
    }
    // (필요 시 다른 인벤토리 → 여기 이동 처리 추가…)

    // 마우스/드래그 상태 정리
    bDraggingItemActive = false;
    bDragArmed = false;

    // 뷰포트 포커스 복구 (있으면 유지)
    if (TSharedPtr<SViewport> VP = GetGameViewportSViewport(GetWorld()))
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedPtr<SWidget>(VP), EFocusCause::SetDirectly);
        FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
    }

    return bResult;
}

// ===================== 더블클릭 → 아이템 사용 =====================

FReply UInventorySlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& Geo, const FPointerEvent& E)
{
    if (E.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        KeepGameInputFocus();   // WASD 끊김 방지

        // ★ 더블클릭 로그
        const int32 Qty = (Item ? Item->Quantity : 0);
        UE_LOG(LogInvDrag, Log, TEXT("[InvSlot] DoubleClick: Slot=%d, Item=%s, Qty=%d"),
            SlotIndex, *GetNameSafe(Item), Qty);

        TryUseThisItem();      // 포션 사용/소모/쿨타임/HP회복
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

        UE_LOG(LogInvDrag, Log, TEXT("[InvSlot] Bound Border delegates (Initialized): this=%p"), this);
    }
    else
    {
        UE_LOG(LogInvDrag, Warning, TEXT("[InvSlot] BorderSlot is NULL (BindWidget 실패?) this=%p"), this);
    }
}

void UInventorySlotWidget::KeepGameInputFocus()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetWidgetToFocus(nullptr); // UI로 포커스 주지 않음
        PC->SetInputMode(Mode);
    }
    if (TSharedPtr<SViewport> VP = GetGameViewportSViewport(GetWorld()))
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedPtr<SWidget>(VP), EFocusCause::SetDirectly);
        FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
    }
}

void UInventorySlotWidget::TryUseThisItem()
{
    if (!OwnerInventory || SlotIndex == INDEX_NONE)
    {
        UE_LOG(LogInvDrag, Warning, TEXT("[InvSlot] TryUseThisItem: Owner/Index invalid (Owner=%s, Index=%d)"),
            *GetNameSafe(OwnerInventory), SlotIndex);
        return;
    }

    // 최신 아이템 포인터 확보 (더블클릭 직전과 다를 수 있음)
    UInventoryItem* Before = OwnerInventory->GetItemAt(SlotIndex);
    if (!Before)
    {
        UE_LOG(LogInvDrag, Warning, TEXT("[InvSlot] TryUseThisItem: No item at Slot=%d"), SlotIndex);
        return;
    }

    const int32 BeforeQty = Before->Quantity;
    const FName BeforeId = Before->ItemId;

    AActor* Context = OwnerInventory->GetOwner();
    const bool bOK = UItemUseLibrary::UseOrEquip(Context ? Context : GetOwningPlayerPawn(), OwnerInventory, SlotIndex);

    // 사용 직후 최신 상태 다시 조회
    UInventoryItem* After = OwnerInventory->GetItemAt(SlotIndex);
    const int32 AfterQty = (After ? After->Quantity : 0);
    const TCHAR* AfterPtr = (After ? TEXT("Valid") : TEXT("Null(Removed)"));


    // 필요하면 즉시 UI 리프레시(보통 델리게이트로 자동 갱신됨)
    // if (bUsed) { RefreshFromInventory(); }
}

// ===================== 데이터/비주얼 갱신 =====================

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
        UE_LOG(LogInvDrag, Verbose, TEXT("[InvSlot] OnSlotUpdated: Slot=%d, Item=%s, Qty=%d"),
            SlotIndex, *GetNameSafe(UpdatedItem), UpdatedItem ? UpdatedItem->Quantity : 0);
        Item = UpdatedItem;
        UpdateVisual();
    }
}

void UInventorySlotWidget::HandleInventoryRefreshed()
{
    UE_LOG(LogInvDrag, Verbose, TEXT("[InvSlot] OnInventoryRefreshed"));
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
            ImgIcon->SetBrush(FSlateBrush()); // ← 리소스 클리어
            ImgIcon->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    if (TxtCount)
    {
        const bool bHasItem = (Item != nullptr);
        int32 Count = bHasItem ? Item->Quantity : 0;

        // 장비면 1이라도 표시하지 않음, 장비가 아니면 1도 표시
        const bool bIsEquipment = bHasItem && Item->IsEquipment();
        const bool bShouldShow =
            (bHasItem && !bIsEquipment && Count >= 1) ||   // 비장비: 1도 표시
            (Count > 1);                                   // (안전망) 2 이상이면 무조건 표시

        if (bShouldShow)
        {
            // 비장비인데 Count==1이면 '1'을 찍고, 나머지는 실제 개수 표시
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
    UE_LOG(LogInvDrag, Log, TEXT("[Slot %d] BORDER MouseDown"), SlotIndex);

    // 1) 우리가 하던 프리뷰 로직(더블클릭/무장/포커스/캡처)
    FReply PreviewReply = NativeOnPreviewMouseButtonDown(MyGeometry, MouseEvent);

    // 2) 엔진 표준 드래그 감지 경로(DetectDragIfPressed 호출 포함)도 태움
    FReply DownReply = NativeOnMouseButtonDown(MyGeometry, MouseEvent);

    // 3) DownReply 를 EventReply 에 실어 반환
    FEventReply Ev = UWidgetBlueprintLibrary::Handled();
    Ev.NativeReply = DownReply;
    return Ev;
}

FEventReply UInventorySlotWidget::OnBorderMouseUp(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
    UE_LOG(LogInvDrag, Log, TEXT("[Slot %d] BORDER MouseUp"), SlotIndex);

    FReply UpReply = NativeOnMouseButtonUp(MyGeometry, MouseEvent);

    FEventReply Ev = UWidgetBlueprintLibrary::Handled();
    Ev.NativeReply = UpReply;
    return Ev;
}

FEventReply UInventorySlotWidget::OnBorderMouseMove(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
    // 마우스 이동 중 드래그 슬롭 넘으면, NativeOnMouseMove 안에서 DetectDragIfPressed → OnDragDetected 호출
    FReply MoveReply = NativeOnMouseMove(MyGeometry, MouseEvent);

    FEventReply Ev = UWidgetBlueprintLibrary::Handled();
    Ev.NativeReply = MoveReply;
    return Ev;
}