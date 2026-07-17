#include "Ability/MMC/MMC_AttackPowerPercent.h"
#include "Ability/NonAttributeSet.h"

UMMC_AttackPowerPercent::UMMC_AttackPowerPercent()
{
    AttackPowerDef.AttributeToCapture = UNonAttributeSet::GetAttackPowerAttribute();
    AttackPowerDef.AttributeSource = EGameplayEffectAttributeCaptureSource::Source;
    AttackPowerDef.bSnapshot = false;

    RelevantAttributesToCapture.Add(AttackPowerDef);
}

float UMMC_AttackPowerPercent::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
    // 1. 공격자(Source)와 피격자(Target)의 태그 정보 가져오기
    FAggregatorEvaluateParameters EvaluationArgs;
    EvaluationArgs.SourceTags = &Spec.CapturedSourceTags.GetActorTags();
    EvaluationArgs.TargetTags = &Spec.CapturedTargetTags.GetActorTags();

    // 2. 공격자의 공격력(AttackPower) 스탯 캡처
    float AttackPower = 0.f;
    GetCapturedAttributeMagnitude(AttackPowerDef, Spec, EvaluationArgs, AttackPower);
    AttackPower = FMath::Max<float>(AttackPower, 0.f);

    // 3. 스킬 데이터 에셋에서 주입받은 데미지 계수(Set By Caller: Data.DamageScale) 가져오기
    // 기본값은 0.0 (즉, 주입 안 되면 데미지 없음)
    float DamageScale = Spec.GetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Data.DamageScale"), false), false, 0.f);

    // 4. 최종 주기당 데미지 = 공격력 * 계수
    return AttackPower * DamageScale;
}
