#include "Ability/NonAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Character/NonCharacterBase.h"
#include "Character/EnemyCharacter.h"
#include "AbilitySystemBlueprintLibrary.h"

static constexpr float AttackSpread = 0.20f; // ±20%
static constexpr float MagicSpread = 0.20f; // ±20%

UNonAttributeSet::UNonAttributeSet()
{
}

void UNonAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, HP, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, MaxHP, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, MP, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, MaxMP, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, SP, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, MaxSP, COND_None, REPNOTIFY_Always);

    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, MinAttackPower, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, MaxAttackPower, COND_None, REPNOTIFY_Always);
    
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, MagicPower, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, MinMagicPower, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, MaxMagicPower, COND_None, REPNOTIFY_Always);
    
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, CriticalRate, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, CriticalDamage, COND_None, REPNOTIFY_Always);
    
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, Defense, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, MagicResist, COND_None, REPNOTIFY_Always);
    
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, CooldownReduction, COND_None, REPNOTIFY_Always);
    
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, HPRegen, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, MPRegen, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, SPRegen, COND_None, REPNOTIFY_Always);

    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, Level, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, Exp, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, ExpToNextLevel, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, StatPoint, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, SkillPoint, COND_None, REPNOTIFY_Always);
}

void UNonAttributeSet::OnRep_HP(const FGameplayAttributeData& OldHP) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, HP, OldHP); }
void UNonAttributeSet::OnRep_MaxHP(const FGameplayAttributeData& OldMaxHP) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, MaxHP, OldMaxHP); }
void UNonAttributeSet::OnRep_MP(const FGameplayAttributeData& OldMP) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, MP, OldMP); }
void UNonAttributeSet::OnRep_MaxMP(const FGameplayAttributeData& OldMaxMP) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, MaxMP, OldMaxMP); }
void UNonAttributeSet::OnRep_SP(const FGameplayAttributeData& OldSP) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, SP, OldSP); }
void UNonAttributeSet::OnRep_MaxSP(const FGameplayAttributeData& OldMaxSP) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, MaxSP, OldMaxSP); }

void UNonAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, AttackPower, OldValue); }
void UNonAttributeSet::OnRep_MinAttackPower(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, MinAttackPower, OldValue); }
void UNonAttributeSet::OnRep_MaxAttackPower(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, MaxAttackPower, OldValue); }
void UNonAttributeSet::OnRep_MagicPower(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, MagicPower, OldValue); }
void UNonAttributeSet::OnRep_MinMagicPower(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, MinMagicPower, OldValue); }
void UNonAttributeSet::OnRep_MaxMagicPower(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, MaxMagicPower, OldValue); }
void UNonAttributeSet::OnRep_CriticalRate(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, CriticalRate, OldValue); }
void UNonAttributeSet::OnRep_CriticalDamage(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, CriticalDamage, OldValue); }
void UNonAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, Defense, OldValue); }
void UNonAttributeSet::OnRep_MagicResist(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, MagicResist, OldValue); }
void UNonAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, MoveSpeed, OldValue); }
void UNonAttributeSet::OnRep_CooldownReduction(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, CooldownReduction, OldValue); }
void UNonAttributeSet::OnRep_HPRegen(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, HPRegen, OldValue); }
void UNonAttributeSet::OnRep_MPRegen(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, MPRegen, OldValue); }
void UNonAttributeSet::OnRep_SPRegen(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, SPRegen, OldValue); }
void UNonAttributeSet::OnRep_Level(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, Level, OldValue); }
void UNonAttributeSet::OnRep_Exp(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, Exp, OldValue); }
void UNonAttributeSet::OnRep_ExpToNextLevel(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, ExpToNextLevel, OldValue); }
void UNonAttributeSet::OnRep_StatPoint(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, StatPoint, OldValue); }
void UNonAttributeSet::OnRep_SkillPoint(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, SkillPoint, OldValue); }


void UNonAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    const FGameplayAttribute& ModifiedAttr = Data.EvaluatedData.Attribute;

    // ── IncomingDamage (DMG 처리) ──
    if (ModifiedAttr == GetIncomingDamageAttribute())
    {
        // GE_Damage가 -10 (음수)을 보낼 수도 있으므로 절대값 처리
        float Damage = FMath::Abs(GetIncomingDamage());
        
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
                    Damage = Reduced; 
                }
            }

            // 최종 HP 차감
            const float OldHP = GetHP();
            const float NewHP = FMath::Clamp(OldHP - Damage, 0.f, GetMaxHP());
            SetHP(NewHP);

            // [New] 사망 처리 (GA_Death 트리거)
            if (NewHP <= 0.f && OldHP > 0.f)
            {
                // Send "Effect.Death" Event
                FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Death"));
                FGameplayEventData Payload;
                Payload.EventTag = DeathTag;
                Payload.Instigator = Data.EffectSpec.GetContext().GetInstigator();
                Payload.Target = Data.Target.GetAvatarActor();
                Payload.EventMagnitude = Damage;

                UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
                    const_cast<AActor*>(Payload.Target.Get()), 
                    DeathTag, 
                    Payload
                );
            }

            // [New] 데미지 표시는 여기서 (최종 데미지 기준)
             if (Damage > 0.1f)
            {
                // ------------- [Fix] GAS 피격 이벤트 공용 발송 (플레이어, 적 모두 포함) -------------
                // 기본 태그: Effect.Hit.Light
                FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit.Light"));
                
                // EffectSpec.DynamicAssetTags에서 "Effect.Hit" 하위 태그가 있는지 확인
                FGameplayTagContainer AssetTags = Data.EffectSpec.GetDynamicAssetTags();
                for (const FGameplayTag& LinkTag : AssetTags)
                {
                    if (LinkTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit"))))
                    {
                        HitEventTag = LinkTag; // Found Specific Tag
                        break;
                    }
                }

                FGameplayEventData Payload;
                Payload.EventTag = HitEventTag;
                Payload.Instigator = Data.EffectSpec.GetContext().GetInstigator();
                Payload.Target = Data.Target.GetAvatarActor();
                Payload.EventMagnitude = Damage;

                // HP가 남아있을 때만 피격 리액션 발생 (죽었을 때는 Death가 전담)
                if (NewHP > 0.f)
                {
                    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(const_cast<AActor*>(Payload.Target.Get()), HitEventTag, Payload);
                }
                // -------------------------------------------------------------------------

                // [New] 플레이어가 적을 쳤을 때 타겟 설정 (UI 표시)
                if (TargetChar == nullptr) // 피해자가 플레이어가 아님 (즉 적)
                {
                     if (ANonCharacterBase* PlayerInstigator = Cast<ANonCharacterBase>(SourceActor))
                     {
                         if (PlayerInstigator->IsLocallyControlled())
                         {
                             if (AEnemyCharacter* VictimEnemy = Cast<AEnemyCharacter>(Data.Target.GetAvatarActor()))
                             {
                                 PlayerInstigator->SetCombatTarget(VictimEnemy);
                             }
                         }
                     }
                }

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