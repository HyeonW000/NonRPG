#include "UI/DraggableWindowBase.h"

#include "Core/BPC_UIManager.h"
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

UDraggableWindowBase::UDraggableWindowBase(const FObjectInitializer& ObjectInitializer)
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
                if (UBPC_UIManager* Found = Pawn->FindComponentByClass<UBPC_UIManager>())
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

        if (IsOnTitleBarExcludingClose(InMouseEvent))
        {
            if (UIManager.IsValid())
            {
                UIManager->BringToFront(this);
            }

            if (UWorld* World = GetWorld())
            {
                FVector2D MousePx, MouseVp;
                USlateBlueprintLibrary::AbsoluteToViewport(World, InMouseEvent.GetScreenSpacePosition(), MousePx, MouseVp);
                DragStartMouseViewport = MouseVp;

                const FGeometry& Geo = GetCachedGeometry();
                FVector2D WinPx, WinVp;
                USlateBlueprintLibrary::AbsoluteToViewport(World, Geo.GetAbsolutePosition(), WinPx, WinVp);
                DragStartWindowViewport = WinVp;

                bDragging = true;

                if (APlayerController* PC = GetOwningPlayer())
                {
                    FInputModeGameAndUI Mode;
                    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                    Mode.SetHideCursorDuringCapture(false);
                    Mode.SetWidgetToFocus(nullptr);
                    PC->SetInputMode(Mode);
                }

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
    }

    return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UDraggableWindowBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && IsOnTitleBarExcludingClose(InMouseEvent))
    {
        if (UIManager.IsValid())
        {
            UIManager->BringToFront(this);
        }

        if (UWorld* World = GetWorld())
        {
            FVector2D MousePx, MouseVp;
            USlateBlueprintLibrary::AbsoluteToViewport(World, InMouseEvent.GetScreenSpacePosition(), MousePx, MouseVp);
            DragStartMouseViewport = MouseVp;

            const FGeometry& Geo = GetCachedGeometry();
            FVector2D WinPx, WinVp;
            USlateBlueprintLibrary::AbsoluteToViewport(World, Geo.GetAbsolutePosition(), WinPx, WinVp);
            DragStartWindowViewport = WinVp;

            bDragging = true;

            if (APlayerController* PC = GetOwningPlayer())
            {
                FInputModeGameAndUI Mode;
                Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                Mode.SetHideCursorDuringCapture(false);
                Mode.SetWidgetToFocus(nullptr);
                PC->SetInputMode(Mode);
            }

            TSharedPtr<SViewport> Viewport = UIViewportUtils::GetGameViewportSViewport(World);
            TSharedPtr<SWidget>   ThisSlate = GetCachedWidget();

            FReply Reply = FReply::Handled();
            if (Viewport.IsValid())
            {
                Reply = Reply.SetUserFocus(StaticCastSharedRef<SWidget>(Viewport.ToSharedRef()), EFocusCause::SetDirectly);
            }
            if (ThisSlate.IsValid())
            {
                Reply = Reply.CaptureMouse(ThisSlate.ToSharedRef());
            }
            return Reply;
        }
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UDraggableWindowBase::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;

        FReply Reply = FReply::Handled().ReleaseMouseCapture();

        if (TSharedPtr<SViewport> Viewport = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
        {
            Reply = Reply.SetUserFocus(StaticCastSharedRef<SWidget>(Viewport.ToSharedRef()), EFocusCause::SetDirectly);
        }

        if (APlayerController* PC = GetOwningPlayer())
        {
            FInputModeGameAndUI Mode;
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Mode.SetHideCursorDuringCapture(false);
            Mode.SetWidgetToFocus(nullptr);
            PC->SetInputMode(Mode);
        }

        return Reply;
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UDraggableWindowBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bDragging) return;

    if (UWorld* World = GetWorld())
    {
        const FVector2D CursorAbs = FSlateApplication::Get().GetCursorPos();
        FVector2D CurPx, CurVp;
        USlateBlueprintLibrary::AbsoluteToViewport(World, CursorAbs, CurPx, CurVp);

        const FVector2D Delta = CurVp - DragStartMouseViewport;

        FVector2D NewPos = DragStartWindowViewport + Delta;

        if (bClampToViewport)
        {
            NewPos = ClampToViewportIfNeeded(NewPos, GetDesiredSize());
        }

        SetAlignmentInViewport(FVector2D(0.f, 0.f));
        SetPositionInViewport(NewPos, /*bRemoveDPIScale=*/false);
    }
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

bool UDraggableWindowBase::ToViewportPos(UWorld* World, const FVector2D& ScreenPos, FVector2D& OutViewportPos)
{
    if (!World) return false;
    FVector2D Pixel(0.f);
    USlateBlueprintLibrary::AbsoluteToViewport(World, ScreenPos, Pixel, OutViewportPos);
    return true;
}

FVector2D UDraggableWindowBase::ClampToViewportIfNeeded(const FVector2D& InPos, const FVector2D& DesiredSize) const
{
    FVector2D Result = InPos;

    if (const APlayerController* PC = GetOwningPlayer())
    {
        int32 SizeX = 0, SizeY = 0;
        PC->GetViewportSize(SizeX, SizeY);

        const float MaxX = FMath::Max(0.f, float(SizeX) - DesiredSize.X);
        const float MaxY = FMath::Max(0.f, float(SizeY) - DesiredSize.Y);

        Result.X = FMath::Clamp(Result.X, 0.f, MaxX);
        Result.Y = FMath::Clamp(Result.Y, 0.f, MaxY);
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
