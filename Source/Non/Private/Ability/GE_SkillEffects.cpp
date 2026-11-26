#include "Ability/GE_SkillEffects.h"
#include "Ability/NonAttributeSet.h"

UGE_SkillCooldown::UGE_SkillCooldown()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;

    FSetByCallerFloat SetByCaller;
    SetByCaller.DataName = NonSkillEffects::CooldownDataName;
    DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}

void UGE_SkillCostBase::InitCostModifier(const FGameplayAttribute& Attribute)
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FSetByCallerFloat SetByCaller;
    SetByCaller.DataName = NonSkillEffects::CostDataName;

    FGameplayModifierInfo Modifier;
    Modifier.Attribute = Attribute;
    Modifier.ModifierOp = EGameplayModOp::AddBase;
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
    Modifiers.Add(Modifier);
}

UGE_SkillCost_SP::UGE_SkillCost_SP()
{
    InitCostModifier(UNonAttributeSet::GetSPAttribute());
}

UGE_SkillCost_MP::UGE_SkillCost_MP()
{
    InitCostModifier(UNonAttributeSet::GetMPAttribute());
}

UGE_SkillCost_HP::UGE_SkillCost_HP()
{
    InitCostModifier(UNonAttributeSet::GetHPAttribute());
}
