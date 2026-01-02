#include "Ability/NonAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Character/NonCharacterBase.h"
#include "AI/EnemyCharacter.h"

static constexpr float AttackSpread = 0.20f; // ±20%
static constexpr float MagicSpread = 0.20f; // ±20%

UNonAttributeSet::UNonAttributeSet()
{
}

void UNonAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    const FGameplayAttribute& ModifiedAttr = Data.EvaluatedData.Attribute;

    // ── IncomingDamage (DMG 처리) ──
    // ── IncomingDamage (DMG 처리) ──
    if (ModifiedAttr == GetIncomingDamageAttribute())
    {
        // GE_Damage가 -10 (음수)을 보낼 수도 있으므로 절대값 처리
        float Damage = FMath::Abs(GetIncomingDamage());
        
        // Debug Log
        // UE_LOG(LogTemp, Warning, TEXT("[NonAttributeSet] IncomingDamage Detected: %.2f (Raw: %.2f)"), Damage, GetIncomingDamage());

        SetIncomingDamage(0.f); // 메타 속성이므로 즉시 초기화

        // 0보다 큰지 체크 (KINDA_SMALL_NUMBER 사용 권장)
        if (Damage > 0.1f)
        {
            ANonCharacterBase* TargetChar = Cast<ANonCharacterBase>(Data.Target.GetAvatarActor());
            AActor* SourceActor = Data.EffectSpec.GetContext().GetInstigator();

            // 가드 중인지 체크
            if (TargetChar && TargetChar->IsGuarding())
            {
                bool bBlocked = true;

                // 정면 판정 (공격자가 있으면)
                if (SourceActor)
                {
                    const FVector ArgVector = (SourceActor->GetActorLocation() - TargetChar->GetActorLocation()).GetSafeNormal();
                    const float DotResult = FVector::DotProduct(TargetChar->GetActorForwardVector(), ArgVector);

                    // Debug Log (삭제됨)
                    // UE_LOG(LogTemp, Warning, TEXT("[GuardCheck] Dot: %.2f (Need > 0.0)"), DotResult);

                    // 내 앞 180도 (-90 ~ +90) 커버 -> Dot > 0
                    if (DotResult < 0.f)
                    {
                        bBlocked = false; // 뒤에서 맞음
                    }
                }

                if (bBlocked)
                {
                    // 데미지 50% 반감
                    float Reduced = Damage * 0.5f;
                    
                    // UE_LOG(LogTemp, Warning, TEXT("[IncomingDamage] Guard Success! Orig: %.1f -> Reduced: %.1f"), Damage, Reduced);
                    Damage = Reduced; 
                }
            }
            else
            {
                // UE_LOG(LogTemp, Log, TEXT("[IncomingDamage] Not Guarding. (IsGuarding=%d)"), TargetChar ? TargetChar->IsGuarding() : 0);
            }

            // 최종 HP 차감
            const float OldHP = GetHP();
            const float NewHP = FMath::Clamp(OldHP - Damage, 0.f, GetMaxHP());
            SetHP(NewHP);

            // [New] 데미지 표시는 여기서 (최종 데미지 기준)
            if (Damage > 0.1f)
            {
                // Critical 여부 확인
                const FGameplayTag CritTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Damage.Critical"), false);
                const bool bIsCritical = Data.EffectSpec.GetDynamicAssetTags().HasTag(CritTag);

                if (TargetChar)
                {
                     const FVector SpawnLoc = TargetChar->GetActorLocation(); 
                     TargetChar->Multicast_SpawnDamageNumber(Damage, SpawnLoc, bIsCritical);
                }
                else if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Data.Target.GetAvatarActor()))
                {
                     const FVector SpawnLoc = Enemy->GetActorLocation(); 
                     Enemy->Multicast_SpawnDamageNumber(Damage, SpawnLoc, bIsCritical);
                }
            }
        }
    }

    // ── HP 클램프 ──
    else if (ModifiedAttr == GetHPAttribute())
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