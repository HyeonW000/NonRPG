#include "Combat/NonDamageHelpers.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"

static constexpr float ArmorConstant = 20.f;   // 튜닝값
static constexpr float MinDamageRatio = 0.1f;

float UNonDamageHelpers::ComputeDamageFromAttributes(AActor* SourceActor, float PowerScale, ENonDamageType DamageType, bool* bOutWasCritical)
{
    if (!SourceActor || PowerScale <= 0.f)
    {
        return 0.f;
    }

    // 실제 ASC/AttributeSet을 갖고 있는 Pawn 찾기
    APawn* Pawn = Cast<APawn>(SourceActor);
    if (!Pawn)
    {
        Pawn = Cast<APawn>(SourceActor->GetInstigator());
    }

    if (!Pawn)
    {
        return 0.f;
    }

    float BaseStat = 0.f;
    float MinStat = 0.f;
    float MaxStat = 0.f;

    // 크리티컬 관련
    float CriticalRate = 0.f; // 확률(0~100)
    float CriticalDamage = 0.f; // 배율(1.0 이상)

    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Pawn))
    {
        if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
        {
            if (const UNonAttributeSet* Attr = ASC->GetSet<UNonAttributeSet>())
            {
                if (DamageType == ENonDamageType::Physical)
                {
                    BaseStat = Attr->GetAttackPower();
                    MinStat = Attr->GetMinAttackPower();
                    MaxStat = Attr->GetMaxAttackPower();
                }
                else // Magical
                {
                    BaseStat = Attr->GetMagicPower();
                    MinStat = Attr->GetMinMagicPower();
                    MaxStat = Attr->GetMaxMagicPower();
                }

                // 여기서 크리티컬 값도 같이 읽어온다
                CriticalRate = Attr->GetCriticalRate();
                CriticalDamage = Attr->GetCriticalDamage();
            }
        }
    }

    // 사용할 스탯 결정 (Min/Max 있으면 랜덤, 아니면 Base)
    float UsedStat = BaseStat;

    if (MinStat > 0.f && MaxStat >= MinStat)
    {
        UsedStat = FMath::RandRange(MinStat, MaxStat);
    }

    if (UsedStat <= 0.f)
    {
        return 0.f;
    }

    // 기본 데미지 = 스탯 × 스킬 계수
    float Damage = UsedStat * PowerScale;

    // 크리 여부 플래그
    bool bCritThisHit = false;

    // 크리티컬 적용 (방어 들어가기 "전" 단계)
    if (CriticalRate > 0.f && CriticalDamage > 1.f)
    {
        const float Roll = FMath::FRandRange(0.f, 100.f); // 0~100
        if (Roll < CriticalRate)
        {
            bCritThisHit = true;
            Damage *= CriticalDamage;

            // 나중에 크리티컬 이펙트 / 데미지 텍스트 색 바꾸고 싶으면
            // 여기에서 "크리티컬이었다"는 플래그를 외부로 넘기는 구조(Out 파라미터)로 확장하면 됨.
        }
    }
    // 여기 추가
    if (bOutWasCritical)
    {
        *bOutWasCritical = bCritThisHit;
    }
    return Damage;
}

float UNonDamageHelpers::ApplyDefenseReduction(AActor* TargetActor, float RawDamage, ENonDamageType DamageType)
{
    if (!TargetActor || RawDamage <= 0.f)
    {
        return 0.f;
    }

    APawn* Pawn = Cast<APawn>(TargetActor);
    if (!Pawn)
    {
        Pawn = Cast<APawn>(TargetActor->GetInstigator());
    }

    if (!Pawn)
    {
        // 타겟이 Pawn/ASC 없으면 방어력 없이 그대로
        return RawDamage;
    }

    float DefenseValue = 0.f;

    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Pawn))
    {
        if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
        {
            if (const UNonAttributeSet* Attr = ASC->GetSet<UNonAttributeSet>())
            {
                if (DamageType == ENonDamageType::Physical)
                {
                    DefenseValue = Attr->GetDefense();
                }
                else
                {
                    DefenseValue = Attr->GetMagicResist();
                }
            }
        }
    }

    if (DefenseValue <= 0.f)
    {
        return RawDamage; // 방어력 없으면 그대로
    }

    // K / (K + Defense)
    const float Factor = ArmorConstant / (ArmorConstant + DefenseValue);
    float Final = RawDamage * Factor;

    // 최소값 보장 (예: 원 데미지의 10%)
    const float MinAllowed = RawDamage * MinDamageRatio;
    if (Final < MinAllowed)
    {
        Final = MinAllowed;
    }

    return Final;
}