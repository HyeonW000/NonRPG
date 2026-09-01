#include "UI/Buff/BuffSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UBuffSlotWidget::InitBuffSlot(FName InBuffId, const FText& InBuffName, UTexture2D* InIcon, float InDuration)
{
    BuffId = InBuffId;
    TotalDuration = InDuration;

    if (Text_BuffName)
    {
        Text_BuffName->SetText(InBuffName);
    }

    if (Image_BuffIcon && InIcon)
    {
        Image_BuffIcon->SetBrushFromTexture(InIcon);
    }

    UpdateRemainingTime(InDuration);

    // 🌟 [등장 애니메이션 자동 재생]
    if (Anim_Appear)
    {
        PlayAnimation(Anim_Appear);
    }

    OnBuffAppear();
}

void UBuffSlotWidget::UpdateRemainingTime(float InRemaining)
{
    if (!Text_RemainingTime) return;

    if (InRemaining <= 0.f)
    {
        Text_RemainingTime->SetText(FText::GetEmpty());
        return;
    }

    // 0초 직전까지는 소수점 노출 없이 올림 정수로 깔끔하게 계산 (15, 14 ... 1)
    const int32 TotalSeconds = FMath::CeilToInt(InRemaining);

    // 60초(1분) 이상일 때는 'n분 n초' 포맷팅
    if (TotalSeconds >= 60)
    {
        const int32 Minutes = TotalSeconds / 60;
        const int32 Seconds = TotalSeconds % 60;
        Text_RemainingTime->SetText(FText::Format(FText::FromString(TEXT("{0}분 {1}초")), Minutes, Seconds));
    }
    else
    {
        // 60초 미만은 'N초' 로 표시 (예: 15초, 14초 ... 1초)
        Text_RemainingTime->SetText(FText::Format(FText::FromString(TEXT("{0}초")), TotalSeconds));
    }
}
