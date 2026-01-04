#include"UI/DraggableWindowBase.h"

#include "Core/NonUIManagerComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"

#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Components/Widget.h"
#include "Engine/GameViewportClient.h"

#include "Widgets/SViewport.h"
#include "Widgets/SWidget.h"

// 공용 유틸 (GetGameViewportSViewport 단일화)
#include "UI/UIViewportUtils.h"

UDraggableWindowBase::UDraggableWindowBase(const FObjectInitializer & ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(false);
}

void UDraggableWindowBase::NativeConstruct()
{
    Super::NativeConstruct();

    if (!UIManager.IsValid())
    {
        if (APlayerController* PC = GetOwningPlayer())
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                if (UNonUIManagerComponent* Found = Pawn->FindComponentByClass<UNonUIManagerComponent>())
                {
                    UIManager = Found;
                }
            }
        }
    }

    if (CloseButton)
    {
        CloseButton->OnClicked.RemoveAll(this);
        CloseButton->OnPressed.RemoveAll(this);
        CloseButton->OnPressed.AddDynamic(this, &UDraggableWindowBase::OnClosePressed);
        CloseButton->OnClicked.AddDynamic(this, &UDraggableWindowBase::OnCloseClicked);

        CloseButton->SetClickMethod(EButtonClickMethod::PreciseClick);
        CloseButton->SetTouchMethod(EButtonTouchMethod::PreciseTap);
        CloseButton->SetPressMethod(EButtonPressMethod::DownAndUp);
    }
    if (TitleText)
    {
        TitleText->SetText(Title);
    }
}

void UDraggableWindowBase::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    if (UIManager.IsValid())
    {
        UIManager->NotifyWindowHover(this, /*bHovered=*/true);
    }
}

void UDraggableWindowBase::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);
    if (UIManager.IsValid())
    {
        UIManager->NotifyWindowHover(this, /*bHovered=*/false);
    }
}

FReply UDraggableWindowBase::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // ── Close 버튼 클릭일 때만 처리 ──
        if (IsOnCloseButton(InMouseEvent))
        {
            if (APlayerController* PC = GetOwningPlayer())
            {
                FInputModeGameAndUI Mode;
                Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                Mode.SetHideCursorDuringCapture(false);
                Mode.SetWidgetToFocus(nullptr);
                PC->SetInputMode(Mode);
            }

            if (TSharedPtr<SViewport> Viewport = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
            {
                const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
                FSlateApplication::Get().SetUserFocus(
                    UserIdx,
                    StaticCastSharedRef<SWidget>(Viewport.ToSharedRef()),
                    EFocusCause::SetDirectly);
            }

            CloseWindow();
            return FReply::Handled();
        }
    }

    // 타이틀바 드래그는 여기서 처리 안 함
    return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UDraggableWindowBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // 왼쪽 버튼 + 타이틀바 클릭 시 드래그 시작
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && IsOnTitleBarExcludingClose(InMouseEvent))
    {
        if (UIManager.IsValid())
        {
            UIManager->BringToFront(this);
        }

        if (UWorld* World = GetWorld())
        {
            // === 마우스 위치 (Viewport Unit) ===
            FVector2D MousePixel, MouseViewport;
            USlateBlueprintLibrary::AbsoluteToViewport(World, InMouseEvent.GetScreenSpacePosition(), MousePixel, MouseViewport);
            DragStartMouseViewport = MouseViewport;   // Viewport Unit 좌표

            // ★ 창 시작 위치는 "우리가 저장해둔 위치" 기준으로 사용 (지오메트리 사용 안 함) - Viewport Unit
            DragStartWindowViewport = HasSavedViewportPos() ? GetSavedViewportPos() : DefaultViewportPos;

            bDragging = true;

            if (APlayerController* PC = GetOwningPlayer())
            {
                FInputModeGameAndUI Mode;
                Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                Mode.SetHideCursorDuringCapture(false);
                Mode.SetWidgetToFocus(nullptr);
                PC->SetInputMode(Mode);
            }

            // Slate 쪽으로 마우스 캡처
            if (TSharedPtr<SViewport> Viewport = UIViewportUtils::GetGameViewportSViewport(World))
            {
                TSharedPtr<SWidget> ThisSlate = GetCachedWidget();

                FReply Reply = FReply::Handled()
                    .SetUserFocus(StaticCastSharedRef<SWidget>(Viewport.ToSharedRef()), EFocusCause::SetDirectly);

                if (ThisSlate.IsValid())
                {
                    Reply = Reply.CaptureMouse(ThisSlate.ToSharedRef());
                }
                return Reply;
            }
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}


FReply UDraggableWindowBase::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;

        if (TSharedPtr<SWidget> ThisSlate = GetCachedWidget())
        {
            return FReply::Handled().ReleaseMouseCapture();
        }

        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UDraggableWindowBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bDragging) return;

    if (UWorld* World = GetWorld())
    {
        // 현재 커서 위치 (절대 → Viewport Unit 좌표)
        const FVector2D CursorAbs = FSlateApplication::Get().GetCursorPos();
        FVector2D CurPixel, CurViewport;
        USlateBlueprintLibrary::AbsoluteToViewport(World, CursorAbs, CurPixel, CurViewport);

        // 드래그 시작 시점 대비 이동량 (Viewport Unit 기준)
        const FVector2D Delta = CurViewport - DragStartMouseViewport;

        // 새 위치 (Viewport Unit 좌표)
        FVector2D NewPos = DragStartWindowViewport + Delta;

        if (bClampToViewport)
        {
            NewPos = ClampToViewportIfNeeded(NewPos);
        }

        // 앵커는 (0,0), 픽셀 좌표 → bRemoveDPIScale = false
        SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
        SetAlignmentInViewport(FVector2D(0.f, 0.f));
        SetPositionInViewport(NewPos, /*bRemoveDPIScale=*/false);

        // ★ 항상 마지막 위치 저장
        SetSavedViewportPos(NewPos);
    }
}

bool UDraggableWindowBase::ToViewportPos(UWorld* World, const FVector2D& ScreenPos, FVector2D& OutViewportPos)
{
    if (!World) return false;

    // 픽셀 좌표를 그대로 쓰도록 통일
    FVector2D Pixel(0.f), Viewport(0.f);
    USlateBlueprintLibrary::AbsoluteToViewport(World, ScreenPos, Pixel, Viewport);
    OutViewportPos = Pixel;
    return true;
}


bool UDraggableWindowBase::IsOnTitleBarExcludingClose(const FPointerEvent& E) const
{
    if (!TitleBarArea) return false;

    const FVector2D Screen = E.GetScreenSpacePosition();

    if (CloseButton)
    {
        const FGeometry& CloseGeo = CloseButton->GetCachedGeometry();
        if (CloseGeo.IsUnderLocation(Screen))
        {
            return false;
        }
    }

    const FGeometry& TitleGeo = TitleBarArea->GetCachedGeometry();
    return TitleGeo.IsUnderLocation(Screen);
}

FVector2D UDraggableWindowBase::ClampToViewportIfNeeded(const FVector2D& InPos) const
{
    FVector2D Result = InPos;

    if (const APlayerController* PC = GetOwningPlayer())
    {
        int32 SizeX = 0, SizeY = 0;
        PC->GetViewportSize(SizeX, SizeY); // 뷰포트 픽셀 사이즈
        
        float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
        if (ViewportScale <= 0.f) ViewportScale = 1.f;

        const FGeometry& Geo = GetCachedGeometry();
        const FVector2D WinSizePx = Geo.GetAbsoluteSize(); // 위젯 절대 픽셀 사이즈

        // 픽셀 기준 여백
        const float MaxX_Px = FMath::Max(0.f, float(SizeX) - WinSizePx.X);
        const float MaxY_Px = FMath::Max(0.f, float(SizeY) - WinSizePx.Y);

        // Viewport Unit으로 변환
        const float MaxX_Unit = MaxX_Px / ViewportScale;
        const float MaxY_Unit = MaxY_Px / ViewportScale;

        Result.X = FMath::Clamp(Result.X, 0.f, MaxX_Unit);
        Result.Y = FMath::Clamp(Result.Y, 0.f, MaxY_Unit);
    }
    return Result;
}

void UDraggableWindowBase::OnClosePressed()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetWidgetToFocus(nullptr);
        PC->SetInputMode(Mode);
    }

    if (TSharedPtr<SViewport> Viewport = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(
            UserIdx,
            StaticCastSharedRef<SWidget>(Viewport.ToSharedRef()),
            EFocusCause::SetDirectly
        );
    }
}

void UDraggableWindowBase::OnCloseClicked()
{
    CloseWindow();

    if (TSharedPtr<SViewport> Viewport = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(
            UserIdx,
            StaticCastSharedRef<SWidget>(Viewport.ToSharedRef()),
            EFocusCause::SetDirectly
        );
    }
}

void UDraggableWindowBase::CloseWindow()
{
    if (UIManager.IsValid())
    {
        UIManager->NotifyWindowClosed(this);
    }
    else
    {
        SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UDraggableWindowBase::BringToFront()
{
    if (UIManager.IsValid())
    {
        UIManager->BringToFront(this);
    }
}

bool UDraggableWindowBase::IsOnCloseButton(const FPointerEvent& E) const
{
    if (!CloseButton) return false;
    const FGeometry& CloseGeo = CloseButton->GetCachedGeometry();
    return CloseGeo.IsUnderLocation(E.GetScreenSpacePosition());
}

void UDraggableWindowBase::SetTitle(const FText& InTitle)
{
    Title = InTitle;

    if (TitleText)
    {
        TitleText->SetText(Title);
    }
    else
    {
        BP_OnTitleChanged(Title);
    }
}

// ----- 위치 저장 함수 구현 -----
void UDraggableWindowBase::SetSavedViewportPos(const FVector2D& InPos)
{
    SavedViewportPos = InPos;
    bHasSavedViewportPos = true;
}