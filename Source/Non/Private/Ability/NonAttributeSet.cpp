#include "Ability/NonAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Character/NonCharacterBase.h"
#include "Character/EnemyCharacter.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Core/NonPlayerController.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"

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
    DOREPLIFETIME_CONDITION_NOTIFY(UNonAttributeSet, MagicPower, COND_None, REPNOTIFY_Always);
    
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
void UNonAttributeSet::OnRep_MagicPower(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNonAttributeSet, MagicPower, OldValue); }
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

        // 이미 대상이 사망한 상태라면 모든 추가 데미지 처리를 무시
        if (GetHP() <= 0.f)
        {
            return;
        }

        // 0보다 큰지 체크 (KINDA_SMALL_NUMBER 사용 권장)
        if (Damage > 0.1f)
        {
            ANonCharacterBase* TargetChar = Cast<ANonCharacterBase>(Data.Target.GetAvatarActor());
            AActor* SourceActor = Data.EffectSpec.GetContext().GetInstigator();
            ANonCharacterBase* SourceChar = Cast<ANonCharacterBase>(SourceActor);

            // 결투 태그 체크
            FGameplayTag DuelTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combat.Dueling"), false);

            // 1. 평화구역 데미지 필터링 (마을 PvP 차단 및 1:1 결투 활성화)
            if (TargetChar && TargetChar->bIsInPeaceZone)
            {
                bool bTargetIsDueling = TargetChar->GetAbilitySystemComponent() && TargetChar->GetAbilitySystemComponent()->HasMatchingGameplayTag(DuelTag);
                bool bSourceIsDueling = SourceChar && SourceChar->GetAbilitySystemComponent() && SourceChar->GetAbilitySystemComponent()->HasMatchingGameplayTag(DuelTag);

                // 둘 다 결투 중인게 아니라면 데미지를 0으로 만듦
                if (!bTargetIsDueling || !bSourceIsDueling)
                {
                    Damage = 0.f;
                }
            }

            // 가드 중인지 체크
            if (Damage > 0.1f && TargetChar && TargetChar->IsGuarding())
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
            float NewHP = OldHP;

            if (Damage > 0.1f)
            {
                NewHP = OldHP - Damage;

                if (TargetChar)
                {
                    // 2. 결투 도중 피 1 남기고 패배 처리하는 예외 룰
                    bool bTargetIsDueling = TargetChar->GetAbilitySystemComponent() && TargetChar->GetAbilitySystemComponent()->HasMatchingGameplayTag(DuelTag);
                    if (bTargetIsDueling && NewHP <= 1.0f)
                    {
                        NewHP = 1.0f;
                        Damage = FMath::Max(0.f, OldHP - 1.0f); // 1.0f만 남게 데미지 제한
                        
                        // 결투 종료 트리거 (서버에서만)
                        if (TargetChar->HasAuthority())
                        {
                            ANonPlayerController* TargetPC = Cast<ANonPlayerController>(TargetChar->GetController());
                            if (TargetPC && TargetPC->CurrentDuelOpponent)
                            {
                                TargetPC->Multicast_EndDuel(TargetPC->CurrentDuelOpponent, TargetPC);
                            }
                        }
                    }
                }

                NewHP = FMath::Clamp(NewHP, 0.f, GetMaxHP());

                // [Fix] 소수점 체력이 남아 UI는 0인데 캐릭터는 안 죽는 현상 방지: 1.0 미만이면 즉사 처리
                if (NewHP > 0.0f && NewHP < 1.0f) {
                    NewHP = 0.0f;
                }

                SetHP(NewHP);

                // 🩸 [Passive Trigger: Bloodthirst / Lifesteal via TriggerEffect GE]
                const FGameplayTag CritTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Damage.Critical"), false);
                const bool bIsCriticalHit = Data.EffectSpec.GetDynamicAssetTags().HasTag(CritTag);

                if (bIsCriticalHit && SourceChar && SourceChar != Data.Target.GetAvatarActor())
                {
                    if (USkillManagerComponent* SourceSkillMgr = SourceChar->FindComponentByClass<USkillManagerComponent>())
                    {
                        if (USkillDataAsset* DA = SourceSkillMgr->GetDataAsset())
                        {
                            for (const auto& Elem : SourceSkillMgr->GetSkillLevelMap())
                            {
                                const FName SkillId = Elem.Key;
                                const int32 SkillLevel = Elem.Value;
                                if (SkillLevel <= 0) continue;

                                if (const FSkillRow* Row = DA->Skills.Find(SkillId))
                                {
                                    // 조건부 발동 이펙트(TriggerEffect: 예 GE_Bloodthirst_Heal)가 등록된 패시브인지 확인
                                    if (Row->Type == ESkillType::Passive && Row->Passive.TriggerEffect)
                                    {
                                        // 확률 검사 (StatusEffectChances 또는 기본 100%)
                                        float Chance = 1.0f;
                                        if (Row->StatusEffect.StatusEffectChances.IsValidIndex(SkillLevel - 1))
                                        {
                                            Chance = Row->StatusEffect.StatusEffectChances[SkillLevel - 1];
                                        }

                                        if (Chance <= 0.001f || FMath::FRand() <= Chance)
                                        {
                                            if (UAbilitySystemComponent* SourceASC = SourceChar->GetAbilitySystemComponent())
                                            {
                                                FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
                                                Ctx.AddInstigator(SourceChar, SourceChar->GetController());

                                                FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(Row->Passive.TriggerEffect, SkillLevel, Ctx);
                                                if (SpecHandle.IsValid())
                                                {
                                                    float Value = 0.f;
                                                    if (Row->Passive.PassiveValues.IsValidIndex(SkillLevel - 1))
                                                    {
                                                        Value = Row->Passive.PassiveValues[SkillLevel - 1];
                                                    }

                                                    if (!Row->Passive.SetByCallerKey.IsNone())
                                                    {
                                                        SpecHandle.Data->SetSetByCallerMagnitude(
                                                            FGameplayTag::RequestGameplayTag(Row->Passive.SetByCallerKey, false), Value);
                                                    }

                                                    SourceASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 🔥 [Berserker System] 공격 성공 시 분노(Rage) 타격 카운터 누적!
                if (SourceChar && SourceChar != Data.Target.GetAvatarActor())
                {
                    if (USkillManagerComponent* SourceSkillMgr = SourceChar->FindComponentByClass<USkillManagerComponent>())
                    {
                        SourceSkillMgr->AddRageHitCount(1);
                    }
                }

                // ── [New] 패시브 상태이상 자동화 처리 (공격자가 데미지를 가했을 때) ──
                AActor* TargetActor = Data.Target.GetAvatarActor();
                if (SourceChar && TargetActor && NewHP > 0.f)
                {
                    FString EffectName = Data.EffectSpec.Def->GetName();

                    // 출혈/화상 데미지 자체가 또 출혈/화상을 유발하는 무한 루프 방지
                    if (!EffectName.Contains(TEXT("Bleed")) && !EffectName.Contains(TEXT("Burn")))
                    {
                        if (USkillManagerComponent* SkillMgr = SourceChar->FindComponentByClass<USkillManagerComponent>())
                        {
                            USkillDataAsset* DA = SkillMgr->GetDataAsset();
                            if (DA)
                            {
                                TMap<FName, int32> SkillMap = SkillMgr->GetSkillLevelMap();
                                for (const auto& Elem : SkillMap)
                                {
                                    FName SkillId = Elem.Key;
                                    int32 SkillLevel = Elem.Value;

                                    if (SkillLevel > 0)
                                    {
                                        if (const FSkillRow* Row = DA->Skills.Find(SkillId))
                                        {
                                            // 패시브이면서 상태이상을 유발하고, 부여할 PassiveEffect가 유효한지 확인
                                            if (Row->Type == ESkillType::Passive && Row->StatusEffect.bHasStatusEffect && Row->Passive.PassiveEffect)
                                            {
                                                // 확률 검사 (배열에 등록되어 있을 때)
                                                float Chance = 1.0f; // 기본 100%
                                                if (Row->StatusEffect.StatusEffectChances.IsValidIndex(SkillLevel - 1))
                                                {
                                                    Chance = Row->StatusEffect.StatusEffectChances[SkillLevel - 1];
                                                }

                                                // 확률 성공 시에만 적용 (0.001f 이상일 때 주사위 롤)
                                                if (Chance <= 0.001f || FMath::FRand() <= Chance)
                                                {
                                                    if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor))
                                                    {
                                                        FGameplayEffectContextHandle Ctx = TargetASC->MakeEffectContext();
                                                        Ctx.AddInstigator(SourceChar, SourceChar->GetController());

                                                        FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(Row->Passive.PassiveEffect, SkillLevel, Ctx);
                                                        if (SpecHandle.IsValid())
                                                        {
                                                            float Duration = Row->StatusEffect.StatusEffectDurations.IsValidIndex(SkillLevel - 1) ? Row->StatusEffect.StatusEffectDurations[SkillLevel - 1] : 0.f;
                                                            float Value = Row->StatusEffect.StatusEffectValues.IsValidIndex(SkillLevel - 1) ? Row->StatusEffect.StatusEffectValues[SkillLevel - 1] : 0.f;

                                                            // 지속시간 및 데미지 계수 주입 (Set By Caller)
                                                            SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Data.Duration"), false), Duration);
                                                            SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Data.DamageScale"), false), Value);

                                                            TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

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
                // [New] 타격의 정확한 위치(HitResult) 가져오기
                FVector ExactHitLoc = FVector::ZeroVector;
                bool bHasHitLoc = false;
                if (const FHitResult* HitRes = Data.EffectSpec.GetContext().GetHitResult())
                {
                    if (!HitRes->ImpactPoint.IsNearlyZero())
                    {
                        ExactHitLoc = HitRes->ImpactPoint;
                        bHasHitLoc = true;
                    }
                }

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

                // HP가 남아있고 출혈/화상 등의 도트 데미지가 아닐 때만 피격 리액션 발생
                FString EffectName = Data.EffectSpec.Def->GetName();
                bool bIsDoTDamage = EffectName.Contains(TEXT("Bleed")) || EffectName.Contains(TEXT("Burn"));
                if (NewHP > 0.f && !bIsDoTDamage)
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
                     const FVector SpawnLoc = bHasHitLoc ? ExactHitLoc : TargetChar->GetActorLocation(); 
                     
                     // [New] 플레이어가 맞았는지 체크
                     if (TargetChar->IsPlayerControlled())
                     {
                         // 플레이어가 맞은 경우 전용 함수나 플래그를 통해 PlayerDamage 전달
                         // (Multicast_SpawnDamageNumber를 확장하거나 내부에서 처리)
                         TargetChar->Multicast_SpawnPlayerDamageNumber(Damage, SpawnLoc);
                     }
                     else
                     {
                         TargetChar->Multicast_SpawnDamageNumber(Damage, SpawnLoc, bIsCritical);
                     }
                }
                else if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Data.Target.GetAvatarActor()))
                {
                     const FVector SpawnLoc = bHasHitLoc ? ExactHitLoc : Enemy->GetActorLocation(); 
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
}