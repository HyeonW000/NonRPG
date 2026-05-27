#include "UI/DamageNumberWidget.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

void UDamageNumberWidget::SetupNumber(float InValue, ENonDamageNumberCategory InCategory, int32 InFontSize)
{
    if (DamageText)
    {
        const int32 IntVal = FMath::RoundToInt(InValue);
        FText TextVal = FText::AsNumber(IntVal);
        DamageText->SetText(TextVal);

        // 기본 폰트 설정
        FSlateFontInfo FontInfo = DamageText->GetFont();
        FontInfo.Size = InFontSize;
        FontInfo.OutlineSettings.OutlineSize = OutlineSize;
        FontInfo.OutlineSettings.OutlineColor = OutlineColor;
        DamageText->SetFont(FontInfo);

        // BP 이벤트를 호출하여 WBP에서 최종 비주얼(색상, 애니메이션 등)을 결정하게 합니다.
        OnSetupVisuals(InValue, TextVal, InCategory);
    }
}

void UDamageNumberWidget::SetupLabel(const FText& InText, ENonDamageNumberCategory InCategory, int32 InFontSize)
{
    if (DamageText)
    {
        DamageText->SetText(InText);

        FSlateFontInfo FontInfo = DamageText->GetFont();
        FontInfo.Size = InFontSize;
        FontInfo.OutlineSettings.OutlineSize = OutlineSize;
        FontInfo.OutlineSettings.OutlineColor = OutlineColor;
        DamageText->SetFont(FontInfo);

        // BP 이벤트를 호출하여 WBP에서 최종 비주얼을 결정하게 합니다.
        OnSetupVisuals(0.f, InText, InCategory);
    }
}