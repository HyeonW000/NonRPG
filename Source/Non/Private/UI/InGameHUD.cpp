#include "UI/InGameHUD.h"
#include "UI/ComboPopupWidget.h" // [New]
#include "Skill/SkillManagerComponent.h" // [New]
#include "UI/QuickSlot/QuickSlotManager.h" // [New]
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

    if (TextBlock_CharacterName) TextBlock_CharacterName->SetText(FText::GetEmpty());



    if (Overlay_TargetFrame == nullptr)
    {

    }
    else
    {

        Overlay_TargetFrame->SetVisibility(ESlateVisibility::Collapsed); // 시작 시 숨김
    }

    if (Overlay_BossFrame)
    {
        Overlay_BossFrame->SetVisibility(ESlateVisibility::Collapsed);
    }

    // [New] 소유자 캐릭터로부터 SkillManagerComponent를 자동으로 찾아 바인딩합니다. (이중 방어 안전장치)
    if (APawn* OwningPawn = GetOwningPlayerPawn())
    {
        if (USkillManagerComponent* SkillMgr = OwningPawn->FindComponentByClass<USkillManagerComponent>())
        {
            BindSkillManager(SkillMgr);
        }
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

    // Boss HP 보간
    if (Overlay_BossFrame && Overlay_BossFrame->GetVisibility() != ESlateVisibility::Collapsed)
    {
        const float TargetBossHP = (BossHP_Max > 0.f) ? (BossHP_Current / BossHP_Max) : 0.f;
        LerpPercent(BossHP_DisplayPercent, TargetBossHP);
        
        if (ProgressBar_BossHP)
        {
            ProgressBar_BossHP->SetPercent(FMath::Clamp(BossHP_DisplayPercent, 0.f, 1.f));
        }
    }

    // [New] 캐스팅 바 실시간 갱신
    if (bIsCasting && ProgressBar_CastingBar && CastTotalDuration > 0.f)
    {
        CastElapsed += InDeltaTime;
        const float Percent = FMath::Clamp(CastElapsed / CastTotalDuration, 0.f, 1.f);
        ProgressBar_CastingBar->SetPercent(Percent);

        if (CastElapsed >= CastTotalDuration)
            StopCasting(); // 자동으로 숨김 (쫬릭 전환은 GA 관할)
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
        const float Percent = (EXP_Max > 0.f) ? (EXP_Current / EXP_Max) * 100.f : 0.f;
        TextBlock_EXP->SetText(
            FText::FromString(FString::Printf(TEXT("%d / %d (%.1f%%)"), CurInt, MaxInt, Percent))
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

// ─────────────────────────────────────────────────────────────────
// [New] 보스 프레임 제어
// ─────────────────────────────────────────────────────────────────
void UInGameHUD::ShowBossFrame(bool bShow, const FString& BossName, float HP, float MaxHP)
{
    if (!Overlay_BossFrame) return;

    if (!bShow)
    {
        Overlay_BossFrame->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    Overlay_BossFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    if (TextBlock_BossName)
    {
        TextBlock_BossName->SetText(FText::FromString(BossName));
    }

    BossHP_Current = FMath::Max(0.f, HP);
    BossHP_Max = FMath::Max(1.f, MaxHP);
    // 띄울 때 애니메이션 없이 바로 채우려면 아래 주석 해제 (지금은 보간됨)
    // BossHP_DisplayPercent = (BossHP_Max > 0.f) ? (BossHP_Current / BossHP_Max) : 0.f;
}

void UInGameHUD::UpdateBossHP(float HP, float MaxHP)
{
    BossHP_Current = FMath::Max(0.f, HP);
    BossHP_Max = FMath::Max(1.f, MaxHP);
}

// ─────────────────────────────────────────────────────────────────
// [New] 캐스팅 바 제어
// ─────────────────────────────────────────────────────────────────
void UInGameHUD::StartCasting(float Duration)
{
    bIsCasting        = true;
    CastTotalDuration = FMath::Max(0.01f, Duration);
    CastElapsed       = 0.f;

    if (ProgressBar_CastingBar)
        ProgressBar_CastingBar->SetPercent(0.f);

    if (Overlay_CastingBar)
        Overlay_CastingBar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UInGameHUD::StopCasting()
{
    bIsCasting  = false;
    CastElapsed = 0.f;

    if (Overlay_CastingBar)
        Overlay_CastingBar->SetVisibility(ESlateVisibility::Collapsed);
}

void UInGameHUD::BindSkillManager(USkillManagerComponent* SkillMgr)
{
    if (!SkillMgr) return;

    CurrentSkillManager = SkillMgr;

    // 이미 바인딩되어 있을 수 있으므로 안전하게 언바인드 후 바인딩을 진행합니다.
    SkillMgr->OnComboWindowChanged.RemoveDynamic(this, &UInGameHUD::OnComboWindowChangedHandler);
    SkillMgr->OnComboWindowChanged.AddDynamic(this, &UInGameHUD::OnComboWindowChangedHandler);
}

void UInGameHUD::OnComboWindowChangedHandler(FName BaseSkillId, FName NextSkillId, float Duration, float CooldownRemaining, float CooldownTotal)
{
    if (!WBP_ComboPopup)
    {
        UE_LOG(LogTemp, Warning, TEXT("[HUD] WBP_ComboPopup 바인딩이 누락되었습니다. 블루프린트 디자이너에서 위젯이 배치되었는지 확인해 주세요."));
        return;
    }

    // [New] 다음 콤보 스킬이 없거나 지속시간이 없거나, 혹은 다음 콤보 스킬의 레벨이 0 이하(미학습)라면 팝업을 띄우지 않고 조기 종료/숨김 처리합니다.
    if (NextSkillId.IsNone() || Duration <= 0.0f || (CurrentSkillManager.IsValid() && CurrentSkillManager->GetSkillLevel(NextSkillId) <= 0))
    {
        WBP_ComboPopup->HideCombo();
        return;
    }

    // [New] 현재 콤보를 유발한 스킬(BaseSkillId)이 등록된 퀵슬롯 번호를 찾아 단축키 텍스트를 구성합니다.
    FText SlotKeyText = FText::GetEmpty();
    if (APawn* OwningPawn = GetOwningPlayerPawn())
    {
        if (UQuickSlotManager* QuickSlotMgr = OwningPawn->FindComponentByClass<UQuickSlotManager>())
        {
            const int32 QuickIndex = QuickSlotMgr->SkillIdsPerSlot.Find(BaseSkillId);
            if (QuickIndex != INDEX_NONE)
            {
                // 0~8번 인덱스는 단축키 1~9번, 9번 인덱스는 단축키 0번으로 매핑합니다.
                const int32 DisplayKey = (QuickIndex == 9) ? 0 : (QuickIndex + 1);
                SlotKeyText = FText::AsNumber(DisplayKey);
            }
        }
    }

    // 약 참조 검사 및 스킬 데이터 에셋을 조회하여 아이콘 소프트 텍스처를 얻어옵니다.
    if (CurrentSkillManager.IsValid())
    {
        if (USkillDataAsset* DataAsset = CurrentSkillManager->GetDataAsset())
        {
            if (FSkillRow* FoundRow = DataAsset->Skills.Find(NextSkillId))
            {
                // C++ 단에서 콤보 이미지 텍스처와 단축키 번호를 변경하고 자동으로 가시성을 노출합니다. (서버 동기화된 쿨다운 정보 다이렉트 전달)
                WBP_ComboPopup->ShowCombo(FoundRow->Icon, Duration, SlotKeyText, CooldownRemaining, CooldownTotal);
            }
            else
            {
            }
        }
    }
}

