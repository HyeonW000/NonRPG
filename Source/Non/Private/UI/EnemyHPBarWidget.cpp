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

    if (Bar) return;

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

    Bar->SetFillColorAndOpacity(FillColor);

    // NOTE: UE5.5에서 WidgetStyle 직접 접근 경고가 뜰 수 있음.
    // ProgressBar는 Background에 대한 공개 setter가 없어 WidgetStyle 수정이 일반적 패턴.
    Bar->WidgetStyle.BackgroundImage.TintColor = BackgroundColor;

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
