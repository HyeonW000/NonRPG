#include "Ability/NonAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

static constexpr float AttackSpread = 0.20f; // ±20%
static constexpr float MagicSpread = 0.20f; // ±20%

UNonAttributeSet::UNonAttributeSet()
{
}

void UNonAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    const FGameplayAttribute& ModifiedAttr = Data.EvaluatedData.Attribute;

    // ── HP 클램프 ──
    if (ModifiedAttr == GetHPAttribute())
    {
        float NewHP = GetHP();
        const float MaxHPValue = GetMaxHP();   // ← 멤버 이름과 안 겹치게

        // Max가 0 이하면 위쪽 클램프는 안 하고, 0 이상만 보장
        if (MaxHPValue > 0.f)
        {
            NewHP = FMath::Clamp(NewHP, 0.f, MaxHPValue);
        }
        else
        {
            NewHP = FMath::Max(NewHP, 0.f);
        }

        SetHP(NewHP);
    }
    // ── MP 클램프 ──
    else if (ModifiedAttr == GetMPAttribute())
    {
        float NewMP = GetMP();
        const float MaxMPValue = GetMaxMP();

        if (MaxMPValue > 0.f)
        {
            NewMP = FMath::Clamp(NewMP, 0.f, MaxMPValue);
        }
        else
        {
            NewMP = FMath::Max(NewMP, 0.f);
        }

        SetMP(NewMP);
    }
    // ── SP 클램프 ──
    else if (ModifiedAttr == GetSPAttribute())
    {
        float NewSP = GetSP();
        const float MaxSPValue = GetMaxSP();

        if (MaxSPValue > 0.f)
        {
            NewSP = FMath::Clamp(NewSP, 0.f, MaxSPValue);
        }
        else
        {
            NewSP = FMath::Max(NewSP, 0.f);
        }

        SetSP(NewSP);
    }

    if (ModifiedAttr == GetAttackPowerAttribute())
    {
        RecalcAttackRangesFromBase();
    }
    else if (ModifiedAttr == GetMagicPowerAttribute())
    {
        RecalcMagicRangesFromBase();
    }
}

// Attack
void UNonAttributeSet::RecalcAttackRangesFromBase()
{
    const float Base = GetAttackPower();
    if (Base <= 0.f)
    {
        // 0 이하이면 Min/Max도 0으로
        SetMinAttackPower(0.f);
        SetMaxAttackPower(0.f);
        return;
    }

    const float Min = Base * (1.f - AttackSpread);
    const float Max = Base * (1.f + AttackSpread);

    SetMinAttackPower(Min);
    SetMaxAttackPower(Max);
}
// Magic
void UNonAttributeSet::RecalcMagicRangesFromBase()
{
    const float Base = GetMagicPower();
    if (Base <= 0.f)
    {
        SetMinMagicPower(0.f);
        SetMaxMagicPower(0.f);
        return;
    }

    const float Min = Base * (1.f - MagicSpread);
    const float Max = Base * (1.f + MagicSpread);

    SetMinMagicPower(Min);
    SetMaxMagicPower(Max);
}