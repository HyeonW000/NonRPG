#include "Ability/Skills/GA_Tranquility.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"
#include "Character/NonCharacterBase.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "UI/InGameHUD.h"
#include "Core/NonUIManagerComponent.h"

UGA_Tranquility::UGA_Tranquility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
    bServerRespectsRemoteAbilityCancellation = true;
    bRetriggerInstancedAbility = false; // [단발 스킬] 시전 도중 연타 시 재발동/캔슬 방지 (이중 잠금)

    // AssetTags (Default AbilityTags)
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Skill")));
    SetAssetTags(AssetTags);

    // Block Abilities with Tag (스킬 시전 중 다른 기본 행동 차단)
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo1")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo2")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo3")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Dodge")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Guard")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.HitReaction")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Skill")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.ToggleWeapon")));

    // Activation Owned Tags
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Skill")));

    // Activation Required Tags (대검 장착 상태)
    ActivationRequiredTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Armed.GreatSword")));

    // Activation Blocked Tags (점프 중, 넉다운 중, 기절/CC 중, 사망 중, 다른 스킬 실행 중 시전 불가)
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Jump")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Knockdown")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.CrowdControl")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Skill")));
}

void UGA_Tranquility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    ANonCharacterBase* CasterChar = Cast<ANonCharacterBase>(GetAvatarActorFromActorInfo());
    if (!CasterChar)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    const UNonAttributeSet* AttrSet = CasterChar->GetAttributeSet();
    if (!ASC || !AttrSet)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    const int32 Level = CurrentSkillLevel; // 1, 2, 3
    const int32 LevelIndex = FMath::Clamp(Level - 1, 0, 2);

    // 1. 💧 [기본 효과] 스태미나(SP) 회복 (데이터 에셋의 Combat.LevelScalars 우선, 없으면 기본 50/60/70)
    float SPRecover = BaseSPRecoverAmounts.IsValidIndex(LevelIndex) ? BaseSPRecoverAmounts[LevelIndex] : 50.f;
    if (CachedRow && CachedRow->Combat.LevelScalars.IsValidIndex(LevelIndex))
    {
        SPRecover = CachedRow->Combat.LevelScalars[LevelIndex];
    }

    const float CurrentSP = AttrSet->GetSP();
    const float MaxSP = AttrSet->GetMaxSP();
    const float NewSP = FMath::Clamp(CurrentSP + SPRecover, 0.f, MaxSP);

    ASC->SetNumericAttributeBase(AttrSet->GetSPAttribute(), NewSP);

    UE_LOG(LogTemp, Warning, TEXT("[GA_Tranquility] SP Recovered: +%.1f (SP: %.1f -> %.1f / %.1f) [Lv.%d]"),
        SPRecover, CurrentSP, NewSP, MaxSP, Level);

    // 2. 🔥 [분노 연계 효과] 분노(State.Rage) 상태 검사
    static const FGameplayTag RageStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
    const bool bIsRageActive = ASC->HasMatchingGameplayTag(RageStateTag);

    if (bIsRageActive)
    {
        // 2-1. 최대 생명력 비례 회복 (데이터 에셋의 StatusEffect.StatusEffectValues 우선, 없으면 기본 10%/15%/20%)
        float HealRatio = RageHealHPRatios.IsValidIndex(LevelIndex) ? RageHealHPRatios[LevelIndex] : 0.10f;
        if (CachedRow && CachedRow->StatusEffect.StatusEffectValues.IsValidIndex(LevelIndex))
        {
            float CustomVal = CachedRow->StatusEffect.StatusEffectValues[LevelIndex];
            // 사용자가 10, 15, 20 처럼 퍼센트로 넣은 경우 0.10, 0.15, 0.20 으로 자동 보정
            if (CustomVal > 1.0f)
            {
                CustomVal /= 100.f;
            }
            HealRatio = CustomVal;
        }

        const float MaxHP = AttrSet->GetMaxHP();
        const float CurrentHP = AttrSet->GetHP();
        const float HealAmount = MaxHP * HealRatio;
        const float NewHP = FMath::Clamp(CurrentHP + HealAmount, 0.f, MaxHP);

        ASC->SetNumericAttributeBase(AttrSet->GetHPAttribute(), NewHP);

        // 플로팅 힐 숫자 출력
        CasterChar->Multicast_SpawnHealNumber(HealAmount, CasterChar->GetActorLocation());

        UE_LOG(LogTemp, Warning, TEXT("[GA_Tranquility] Rage Cleansed & HP Healed: +%.1f (%.0f%%) (HP: %.1f -> %.1f / %.1f) [Lv.%d]"),
            HealAmount, HealRatio * 100.f, CurrentHP, NewHP, MaxHP, Level);

        // 2-2. 활성화된 분노 상태 및 버프 즉시 완전 해제!
        if (USkillManagerComponent* SkillMgr = CasterChar->FindComponentByClass<USkillManagerComponent>())
        {
            SkillMgr->DeactivateRageState();
        }

        // 🔥 State.Rage 태그를 부여하고 있는 모든 액티브 GameplayEffect(GE_Berserk 등)를 즉시 ASC에서 완전 제거!
        FGameplayTagContainer RageContainer;
        RageContainer.AddTag(RageStateTag);
        ASC->RemoveActiveEffectsWithGrantedTags(RageContainer);

        // 잔여 LooseTag 제거
        ASC->RemoveLooseGameplayTag(RageStateTag);

        // HUD 버프창에서도 잔여 버프 아이콘 완벽 제거
        if (UNonUIManagerComponent* UIMgr = CasterChar->FindComponentByClass<UNonUIManagerComponent>())
        {
            if (UInGameHUD* HUD = UIMgr->GetInGameHUD())
            {
                HUD->RemoveBuff(FName("State.Rage"));
                HUD->RemoveBuff(FName("B_A_Berserk"));
                HUD->RemoveBuff(FName("Berserk"));
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("[GA_Tranquility] Rage & Buffs COMPLETELY Cleansed! State.Rage Remaining: %s"),
            ASC->HasMatchingGameplayTag(RageStateTag) ? TEXT("TRUE (ERROR)") : TEXT("FALSE (CLEARED)"));
    }

    // 3. 몽타주가 없는 경우 즉시 종료 (몽타주가 있다면 Super::ActivateAbility -> PlayShootMontage() 완료 시 종료)
    if (!CachedRow || !CachedRow->Combat.Montage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}
