#include "UI/DraggableWindowBase.h"

#include "Core/BPC_UIManager.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h" 

#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Components/Widget.h"
#include "Engine/GameViewportClient.h"

#include "Widgets/SViewport.h" 
#include "Widgets/SWidget.h"

UDraggableWindowBase::UDraggableWindowBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(false);
}

void UDraggableWindowBase::NativeConstruct()
{
    Super::NativeConstruct();

    // 소유 Pawn에서 UIManager 캐시
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

    // 닫기 버튼 바인딩(옵션) — 포커스 함수 호출 제거
    if (CloseButton)
    {
        CloseButton->OnClicked.RemoveAll(this);
        CloseButton->OnPressed.RemoveAll(this);            
        CloseButton->OnPressed.AddDynamic(this, &UDraggableWindowBase::OnClosePressed);
        CloseButton->OnClicked.AddDynamic(this, &UDraggableWindowBase::OnCloseClicked);

        // 드래그와 충돌 최소화를 위한 클릭 동작 설정
        CloseButton->SetClickMethod(EButtonClickMethod::PreciseClick);
        CloseButton->SetTouchMethod(EButtonTouchMethod::PreciseTap);
        CloseButton->SetPressMethod(EButtonPressMethod::DownAndUp);
    }
    // 처음 생성될 때 Title 반영
    if (TitleText)
    {
        TitleText->SetText(Title);
    }
}


static TSharedPtr<SViewport> GetGameViewportSViewport(UWorld* World)
{
    if (!World) return nullptr;
    if (UGameViewportClient* GVC = World->GetGameViewport())
    {
        return GVC->GetGameViewportWidget(); // TSharedPtr<SViewport>
    }
    return nullptr;
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

// ── 드래그 시작(우선 Preview에서 처리)
FReply UDraggableWindowBase::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // 1) CloseButton 위에서 시작되면 버튼보다 먼저 처리해서 포커스 탈취 차단
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

            if (UWorld* World = GetWorld())
            {
                if (TSharedPtr<SViewport> Viewport = GetGameViewportSViewport(World))
                {
                    const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
                    FSlateApplication::Get().SetUserFocus(
                        UserIdx,
                        StaticCastSharedRef<SWidget>(Viewport.ToSharedRef()),
                        EFocusCause::SetDirectly);
                }
            }

            CloseWindow();
            return FReply::Handled(); // 버튼으로 이벤트가 넘어가지 않게 종료
        }

        // 2) 타이틀바(닫기 제외)에서 드래그 시작
        if (IsOnTitleBarExcludingClose(InMouseEvent))
        {
            if (UIManager.IsValid())
            {
                UIManager->BringToFront(this);
            }

            if (UWorld* World = GetWorld())
            {
                // 시작 지점 기록
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

                // 게임뷰포트에 사용자 포커스 고정 + 이 위젯으로 마우스 캡처
                if (TSharedPtr<SViewport> Viewport = GetGameViewportSViewport(World))
                {
                    TSharedPtr<SWidget> ThisSlate = GetCachedWidget();

                    FReply Reply = FReply::Handled()
                        .SetUserFocus(
                            StaticCastSharedRef<SWidget>(Viewport.ToSharedRef()),
                            EFocusCause::SetDirectly);

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




// ── Preview를 못 받는 케이스 대비(동일 로직)
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
                Mode.SetWidgetToFocus(nullptr); // <- 핵심
                PC->SetInputMode(Mode);
            }

            TSharedPtr<SViewport> Viewport = GetGameViewportSViewport(World);
            TSharedPtr<SWidget>   ThisSlate = GetCachedWidget();

            FReply Reply = FReply::Handled();
            if (Viewport.IsValid())
            {
                Reply = Reply.SetUserFocus(
                    StaticCastSharedRef<SWidget>(Viewport.ToSharedRef()),
                    EFocusCause::SetDirectly);
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

        if (TSharedPtr<SViewport> Viewport = GetGameViewportSViewport(GetWorld()))
        {
            Reply = Reply.SetUserFocus(
                StaticCastSharedRef<SWidget>(Viewport.ToSharedRef()),
                EFocusCause::SetDirectly);
        }

        // (선택) 여기서도 한 번 더 InputMode 유지
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


// ── 이동: Tick에서 계속 추적(이벤트 유실/커서 창 밖에서도 부드럽게)
void UDraggableWindowBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bDragging) return;

    if (UWorld* World = GetWorld())
    {
        // 현재 커서의 "절대 스크린 좌표" → 뷰포트 좌표로 변환
        const FVector2D CursorAbs = FSlateApplication::Get().GetCursorPos();
        FVector2D CurPx, CurVp;
        USlateBlueprintLibrary::AbsoluteToViewport(World, CursorAbs, CurPx, CurVp);

        // Δ = 현재마우스 - 시작마우스
        const FVector2D Delta = CurVp - DragStartMouseViewport;

        // 새 창 위치 = 시작 창 위치 + Δ
        FVector2D NewPos = DragStartWindowViewport + Delta;

        if (bClampToViewport)
        {
            NewPos = ClampToViewportIfNeeded(NewPos, GetDesiredSize());
        }

        SetAlignmentInViewport(FVector2D(0.f, 0.f));
        SetPositionInViewport(NewPos, /*bRemoveDPIScale=*/false);
    }
}

// ───────── Helpers ─────────
bool UDraggableWindowBase::IsOnTitleBarExcludingClose(const FPointerEvent& E) const
{
    if (!TitleBarArea) return false;

    const FVector2D Screen = E.GetScreenSpacePosition();

    // 1) CloseButton 영역이면 드래그 시작 금지
    if (CloseButton)
    {
        const FGeometry& CloseGeo = CloseButton->GetCachedGeometry();
        if (CloseGeo.IsUnderLocation(Screen))
        {
            return false;
        }
    }

    // 2) TitleBar 영역이면 드래그 허용
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

    if (TSharedPtr<SViewport> Viewport = GetGameViewportSViewport(GetWorld())) // ★ SViewport로 받기
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(
            UserIdx,
            StaticCastSharedPtr<SWidget>(Viewport), // ★ SViewport -> SWidget 캐스팅
            EFocusCause::SetDirectly
        );
    }
}

void UDraggableWindowBase::OnCloseClicked()
{
    CloseWindow();

    if (TSharedPtr<SViewport> Viewport = GetGameViewportSViewport(GetWorld())) // ★ SViewport로 받기
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(
            UserIdx,
            StaticCastSharedPtr<SWidget>(Viewport), // ★ 캐스팅
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
        // 텍스트 위젯을 Base에서 직접 바인딩하지 않는 창이라면,
        // BP에서 이 이벤트를 받아 자체적으로 반영하세요.
        BP_OnTitleChanged(Title);
    }
}