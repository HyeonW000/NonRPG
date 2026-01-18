#include "UI/InGameHUD.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h" 
#include "Components/Overlay.h" // [New]

UInGameHUD::UInGameHUD(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UInGameHUD::NativeConstruct()
{
    Super::NativeConstruct();

    // 초기 값 세팅
    HP_Current = MP_Current = SP_Current = 0.f;
    HP_Max = MP_Max = SP_Max = 1.f;
    HP_DisplayPercent = MP_DisplayPercent = SP_DisplayPercent = 0.f;

    EXP_Current = 0.f;
    EXP_Max = 1.f;
    EXP_DisplayPercent = 0.f;

    CurrentLevel = 1;

    // 초기 바/텍스트 0으로
    if (ProgressBar_HP)  ProgressBar_HP->SetPercent(0.f);
    if (ProgressBar_MP)  ProgressBar_MP->SetPercent(0.f);
    if (ProgressBar_SP)  ProgressBar_SP->SetPercent(0.f);
    if (ProgressBar_EXP) ProgressBar_EXP->SetPercent(0.f);

    if (TextBlock_HPAmount)  TextBlock_HPAmount->SetText(FText::FromString(TEXT("0 / 0")));
    if (TextBlock_MPAmount)  TextBlock_MPAmount->SetText(FText::FromString(TEXT("0 / 0")));
    if (TextBlock_SPAmount)  TextBlock_SPAmount->SetText(FText::FromString(TEXT("0 / 0")));
    if (TextBlock_EXP)       TextBlock_EXP->SetText(FText::FromString(TEXT("0 / 0")));
    if (TextBlock_Level)     TextBlock_Level->SetText(FText::FromString(TEXT("Lv. 1")));

    if (TextBlock_HP) TextBlock_HP->SetText(FText::FromString(TEXT("HP")));
    if (TextBlock_MP) TextBlock_MP->SetText(FText::FromString(TEXT("MP")));
    if (TextBlock_SP) TextBlock_SP->SetText(FText::FromString(TEXT("SP")));

    if (TextBlock_CharacterName) TextBlock_CharacterName->SetText(FText::GetEmpty());



    if (Overlay_TargetFrame == nullptr)
    {

    }
    else
    {

        Overlay_TargetFrame->SetVisibility(ESlateVisibility::Collapsed); // 시작 시 숨김
    }
}

void UInGameHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const float Speed = BarLerpSpeed;

    auto LerpPercent = [Speed, InDeltaTime](float& Display, float Target)
        {
            Display = FMath::FInterpTo(Display, Target, InDeltaTime, Speed);
        };

    // Target 퍼센트 계산
    const float TargetHP = (HP_Max > 0.f) ? (HP_Current / HP_Max) : 0.f;
    const float TargetMP = (MP_Max > 0.f) ? (MP_Current / MP_Max) : 0.f;
    const float TargetSP = (SP_Max > 0.f) ? (SP_Current / SP_Max) : 0.f;
    const float TargetEXP = (EXP_Max > 0.f) ? (EXP_Current / EXP_Max) : 0.f;

    // 보간
    LerpPercent(HP_DisplayPercent, TargetHP);
    LerpPercent(MP_DisplayPercent, TargetMP);
    LerpPercent(SP_DisplayPercent, TargetSP);
    LerpPercent(EXP_DisplayPercent, TargetEXP);

    // 바에 적용
    if (ProgressBar_HP)
    {
        ProgressBar_HP->SetPercent(FMath::Clamp(HP_DisplayPercent, 0.f, 1.f));
    }
    if (ProgressBar_MP)
    {
        ProgressBar_MP->SetPercent(FMath::Clamp(MP_DisplayPercent, 0.f, 1.f));
    }
    if (ProgressBar_SP)
    {
        ProgressBar_SP->SetPercent(FMath::Clamp(SP_DisplayPercent, 0.f, 1.f));
    }
    if (ProgressBar_EXP)
    {
        ProgressBar_EXP->SetPercent(FMath::Clamp(EXP_DisplayPercent, 0.f, 1.f));
    }
}

void UInGameHUD::UpdateHP(float Current, float Max)
{
    HP_Current = FMath::Max(0.f, Current);
    HP_Max = FMath::Max(1.f, Max);

    if (TextBlock_HPAmount)
    {
        const int32 CurInt = FMath::RoundToInt(HP_Current);
        const int32 MaxInt = FMath::RoundToInt(HP_Max);
        TextBlock_HPAmount->SetText(
            FText::FromString(FString::Printf(TEXT("%d / %d"), CurInt, MaxInt))
        );
    }
}

void UInGameHUD::UpdateMP(float Current, float Max)
{
    MP_Current = FMath::Max(0.f, Current);
    MP_Max = FMath::Max(1.f, Max);

    if (TextBlock_MPAmount)
    {
        const int32 CurInt = FMath::RoundToInt(MP_Current);
        const int32 MaxInt = FMath::RoundToInt(MP_Max);
        TextBlock_MPAmount->SetText(
            FText::FromString(FString::Printf(TEXT("%d / %d"), CurInt, MaxInt))
        );
    }
}

void UInGameHUD::UpdateSP(float Current, float Max)
{
    SP_Current = FMath::Max(0.f, Current);
    SP_Max = FMath::Max(1.f, Max);

    if (TextBlock_SPAmount)
    {
        const int32 CurInt = FMath::RoundToInt(SP_Current);
        const int32 MaxInt = FMath::RoundToInt(SP_Max);
        TextBlock_SPAmount->SetText(
            FText::FromString(FString::Printf(TEXT("%d / %d"), CurInt, MaxInt))
        );
    }
}

void UInGameHUD::UpdateEXP(float Current, float Max)
{
    EXP_Current = FMath::Max(0.f, Current);
    EXP_Max = FMath::Max(1.f, Max);

    if (TextBlock_EXP)
    {
        const int32 CurInt = FMath::RoundToInt(EXP_Current);
        const int32 MaxInt = FMath::RoundToInt(EXP_Max);
        TextBlock_EXP->SetText(
            FText::FromString(FString::Printf(TEXT("%d / %d"), CurInt, MaxInt))
        );
    }
}

void UInGameHUD::UpdateLevel(int32 NewLevel)
{
    CurrentLevel = FMath::Max(1, NewLevel);

    if (TextBlock_Level)
    {
        TextBlock_Level->SetText(FText::FromString(FString::Printf(TEXT("%d"), CurrentLevel)));
    }
}

void UInGameHUD::UpdateCharacterName(const FString & NewName)
{
    if (TextBlock_CharacterName)
    {
        TextBlock_CharacterName->SetText(FText::FromString(NewName));
    }
}

void UInGameHUD::UpdateClassIcon(UTexture2D* NewIcon)
{
    if (Image_ClassIcon)
    {
        if (NewIcon)
        {
            Image_ClassIcon->SetBrushFromTexture(NewIcon);
            Image_ClassIcon->SetVisibility(ESlateVisibility::Visible);

        }
        else
        {
            Image_ClassIcon->SetVisibility(ESlateVisibility::Hidden);

        }
    }
    else
    {

    }
}

void UInGameHUD::UpdateTargetInfo(bool bShow, const FString& Name, float HP, float MaxHP, float Distance)
{
    if (!Overlay_TargetFrame) return;

    if (!bShow)
    {
        Overlay_TargetFrame->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    Overlay_TargetFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);



    // 이름
    if (TextBlock_TargetName)
    {
        TextBlock_TargetName->SetText(FText::FromString(Name));
    }

    // HP 바
    if (ProgressBar_TargetHP && MaxHP > 0.f)
    {
        const float Percent = FMath::Clamp(HP / MaxHP, 0.f, 1.f);
        ProgressBar_TargetHP->SetPercent(Percent);
    }

    // 거리
    if (TextBlock_TargetDistance)
    {
        // 미터(m) 단위 변환
        TextBlock_TargetDistance->SetText(FText::FromString(FString::Printf(TEXT("%.1f m"), Distance / 100.f)));
    }
}


