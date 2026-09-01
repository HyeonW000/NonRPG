#pragma once

#include "CoreMinimal.h"
#include "Ability/Skills/GA_SkillBase.h"
#include "GA_Berserk.generated.h"

/**
 * ⚡ 버서커 [광폭화] 버프 스킬 (GA_Berserk)
 * - 시전 시 즉시 '분노'(State.Rage / 15초간 치명타+10%) 버프 획득!
 * - 반경 3~5m 주변 적들에게 피격 이벤트를 쏘아 몽타주 비틀거림/경직(Stagger) 연출!
 * - 반경 10m 아군 파티원들에게 10초간 물리/마법 공격력 +10% 버프 부여!
 */
UCLASS()
class NON_API UGA_Berserk : public UGA_SkillBase
{
    GENERATED_BODY()

public:
    UGA_Berserk();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
    /** 주변 적 비틀거림/경직 반경 (cm) - 기본 400cm (4m) */
    UPROPERTY(EditDefaultsOnly, Category = "Berserk|Stagger")
    float StaggerRadius = 400.f;

    /** 아군 버프 부여 반경 (cm) - 기본 1000cm (10m) */
    UPROPERTY(EditDefaultsOnly, Category = "Berserk|PartyBuff")
    float PartyBuffRadius = 1000.f;

    /** 10m 아군 전용 물리/마법 공격력 +10% 버프 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, Category = "Berserk|PartyBuff")
    TSubclassOf<class UGameplayEffect> PartyBuffEffectClass;
};
