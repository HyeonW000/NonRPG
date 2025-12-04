#include "UI/DamageNumberWidget.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

void UDamageNumberWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildIfNeeded();
}

void UDamageNumberWidget::BuildIfNeeded()
{
    if (bBuilt) return;
    bBuilt = true;

    if (!WidgetTree)
    {
        return;
    }

    // 루트 캔버스
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // 텍스트
    DamageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DamageText"));
    if (DamageText)
    {
        DamageText->SetJustification(ETextJustify::Center);
        DamageText->SetAutoWrapText(false);
        DamageText->SetShadowOffset(FVector2D(1.f, 1.f));
        DamageText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));

        // 중앙 배치 (변수명: CanvasSlot 로 변경해 C4458 방지)
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Root->AddChild(DamageText)))
        {
            CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
            CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
            CanvasSlot->SetAutoSize(true);
        }
    }
}

void UDamageNumberWidget::SetupNumber(float InValue, const FLinearColor& InColor, int32 InFontSize)
{
    PendingValue = InValue;
    PendingColor = InColor;
    PendingFontSize = InFontSize;

    if (!DamageText)
    {
        BuildIfNeeded();
    }
    if (!DamageText) return;

    const int32 IntVal = FMath::RoundToInt(InValue);
    DamageText->SetText(FText::AsNumber(IntVal));

    // 폰트 + 아웃라인
    FSlateFontInfo FontInfo = DamageText->GetFont();
    FontInfo.Size = InFontSize;

    FFontOutlineSettings OL;
    OL.OutlineSize = OutlineSize;          //  외곽선 두께
    OL.OutlineColor = OutlineColor;        //  외곽선 색 (기본 검정)
    FontInfo.OutlineSettings = OL;

    DamageText->SetFont(FontInfo);

    DamageText->SetColorAndOpacity(InColor);
}

void UDamageNumberWidget::SetOutline(int32 InSize, FLinearColor InColor)
{
    OutlineSize = InSize;
    OutlineColor = InColor;

    if (DamageText)
    {
        FSlateFontInfo FontInfo = DamageText->GetFont();
        FFontOutlineSettings OL;
        OL.OutlineSize = OutlineSize;
        OL.OutlineColor = OutlineColor;
        FontInfo.OutlineSettings = OL;
        DamageText->SetFont(FontInfo);
    }
}

void UDamageNumberWidget::SetupLabel(const FText& InText, const FLinearColor& InColor, int32 InFontSize)
{
    PendingColor = InColor;
    PendingFontSize = InFontSize;

    BuildIfNeeded();
    if (!DamageText) return;

    DamageText->SetText(InText);

    FSlateFontInfo FontInfo = DamageText->GetFont();
    FontInfo.Size = InFontSize;

    FFontOutlineSettings OL;
    OL.OutlineSize = OutlineSize;
    OL.OutlineColor = OutlineColor;
    FontInfo.OutlineSettings = OL;

    DamageText->SetFont(FontInfo);
    DamageText->SetColorAndOpacity(InColor);
}