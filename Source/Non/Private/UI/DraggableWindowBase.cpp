#include "UI/DraggableWindowBase.h"

#include "Core/NonUIManagerComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

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
    if (TitleIcon && TitleIconTexture)
    {
        TitleIcon->SetBrushFromTexture(TitleIconTexture);
    }

    // 1. 창을 닫았다가 다시 열었을 때 마지막 드래그 위치 복원
    if (LastWindowPos.X >= 0.f && LastWindowPos.Y >= 0.f)
    {
        SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
        SetAlignmentInViewport(FVector2D(0.f, 0.f));
        SetPositionInViewport(LastWindowPos, /*bRemoveDPIScale=*/false);
    }
    // 2. 드래그 이력은 없지만, 기본 초기 위치가 유효하게 지정되어 있다면 해당 위치 적용
    else if (DefaultWindowPos.X >= 0.f && DefaultWindowPos.Y >= 0.f)
    {
        SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
        SetAlignmentInViewport(FVector2D(0.f, 0.f));
        SetPositionInViewport(DefaultWindowPos, /*bRemoveDPIScale=*/false);
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

    return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UDraggableWindowBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // 왼쪽 마우스 클릭 + 타이틀바 조작 시 드래그 시작
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && IsOnTitleBarExcludingClose(InMouseEvent))
    {
        if (UWorld* World = GetWorld())
        {
            FVector2D DummyPixel;
            
            // [중요] BringToFront() 호출로 인해 RemoveFromParent -> AddToViewport가 일어날 때 
            // Slate 지오메트리 캐시가 초기화되므로, ZOrder를 올리기 전에 현재 위젯의 실제 뷰포트 위치를 먼저 캐싱합니다.
            USlateBlueprintLibrary::AbsoluteToViewport(World, InGeometry.GetAbsolutePosition(), DummyPixel, DragStartWindowPos);
            
            // 드래그 시작 시 마우스 뷰포트 좌표 구하기
            USlateBlueprintLibrary::AbsoluteToViewport(World, InMouseEvent.GetScreenSpacePosition(), DummyPixel, DragStartMousePos);
        }

        // ZOrder 갱신
        if (UIManager.IsValid())
        {
            UIManager->BringToFront(this);
        }

        // 뷰포트에 다시 붙은 직후 원래 위치로 즉시 설정하여 1프레임 깜빡임(0,0으로 튐) 방지
        SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
        SetAlignmentInViewport(FVector2D(0.f, 0.f));
        SetPositionInViewport(DragStartWindowPos, /*bRemoveDPIScale=*/false);

        if (UWorld* World = GetWorld())
        {
            bDragging = true;

            if (APlayerController* PC = GetOwningPlayer())
            {
                FInputModeGameAndUI Mode;
                Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                Mode.SetHideCursorDuringCapture(false);
                Mode.SetWidgetToFocus(nullptr);
                PC->SetInputMode(Mode);
            }

            // 마우스 캡처 시작
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
        // 현재 절대 커서 좌표 ➡️ 뷰포트 좌표 변환
        const FVector2D CursorAbs = FSlateApplication::Get().GetCursorPos();
        FVector2D DummyPixel, CurMouseViewport;
        USlateBlueprintLibrary::AbsoluteToViewport(World, CursorAbs, DummyPixel, CurMouseViewport);

        // 드래그 시작 시점 대비 마우스 이동 델타값
        const FVector2D Delta = CurMouseViewport - DragStartMousePos;

        // 최종 새 위치 계산
        const FVector2D NewPos = DragStartWindowPos + Delta;

        LastWindowPos = NewPos; // 드래그 중인 최종 위치 저장

        // 위치 적용 (DPI 스케일링이 알아서 처리되도록 bRemoveDPIScale=false 설정)
        SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
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

void UDraggableWindowBase::SetTitleIcon(UTexture2D* InTexture)
{
    TitleIconTexture = InTexture;
    if (TitleIcon)
    {
        TitleIcon->SetBrushFromTexture(InTexture);
    }
}