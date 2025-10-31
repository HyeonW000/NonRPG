#include "Ability/GE_StatInit.h"
#include "Ability/NonAttributeSet.h"

UGE_StatInit::UGE_StatInit()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    // 예시: 초기 스탯 정의
    //FGameplayModifierInfo HPModifier;
    //HPModifier.Attribute = UNonAttributeSet::GetHPAttribute();
    //HPModifier.ModifierOp = EGameplayModOp::Additive;
    //HPModifier.ModifierMagnitude = FScalableFloat(100.f);
    //Modifiers.Add(HPModifier);


    // 추가 스탯 필요 시 여기에 계속 추가 가능
}
