#include "UI/EnemyHPBarWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Blueprint/WidgetTree.h"

void UEnemyHPBarWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildIfNeeded();
    ApplyStyle();
}

void UEnemyHPBarWidget::BuildIfNeeded()
{
    if (bBuilt) return;
    bBuilt = true;

    if (!WidgetTree) return;

    // BP에서 ProgressBar(이름 'Bar')를 배치했다면 이미 Bar가 채워져 있음 → 새로 만들지 않음
    if (Bar) return;

    // 없으면 코드로 생성 (지금까지 쓰던 방식)
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar"));
    if (Bar)
    {
        if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Root->AddChild(Bar)))
        {
            S->SetAnchors(FAnchors(0.5f, 0.5f));
            S->SetAlignment(FVector2D(0.5f, 0.5f));
            S->SetAutoSize(false);
            S->SetSize(FVector2D(Width, Height));
        }
    }
}

void UEnemyHPBarWidget::ApplyStyle()
{
    if (!Bar) return;

    // 채움 색상
    Bar->SetFillColorAndOpacity(FillColor);

    // 배경 색상: ProgressBar는 SetBackgroundColor가 없음 → WidgetStyle을 수정
    // BackgroundImage.TintColor 를 원하는 색으로 세팅
    Bar->WidgetStyle.BackgroundImage.TintColor = BackgroundColor;

    // (선택) FillImage의 기본 Tint도 건드리고 싶다면 아래 주석 해제
    // Bar->WidgetStyle.FillImage.TintColor = FillColor;

    // 스타일 변경을 반영
    Bar->SynchronizeProperties();
}

void UEnemyHPBarWidget::SetBarColors(FLinearColor InFill, FLinearColor InBackground)
{
    FillColor = InFill;
    BackgroundColor = InBackground;
    ApplyStyle();
}

void UEnemyHPBarWidget::SetHP(float Current, float Max)
{
    BuildIfNeeded();
    if (!Bar) return;

    const float Den = FMath::Max(1.f, Max);
    const float P = FMath::Clamp(Current / Den, 0.f, 1.f);
    Bar->SetPercent(P);
}
