#pragma once

#include "CoreMinimal.h"
#include "Ability/Skills/GA_SkillBase.h"
#include "GA_Tranquility.generated.h"

/**
 * 🧘‍♂️ [평온 / 평정] 액티브 스킬 (GA_Tranquility)
 * - 기본 효과: 스킬 레벨(1/2/3)에 따라 스태미나(SP) 50 / 60 / 70 회복
 * - 분노(State.Rage) 연계 효과:
 *   - 캐릭터에게 '분노' 상태가 활성화되어 있을 때 시전 시:
 *     - 최대 생명력(MaxHP)의 10% / 15% / 20% 즉시 회복
 *     - 활성화된 분노 상태 및 버프(State.Rage) 즉시 해제
 */
UCLASS()
class NON_API UGA_Tranquility : public UGA_SkillBase
{
    GENERATED_BODY()

public:
    UGA_Tranquility();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

protected:
    /** 레벨별 기본 스태미나(SP) 회복량 (1레벨 50, 2레벨 60, 3레벨 70) */
    UPROPERTY(EditDefaultsOnly, Category = "Tranquility|Recovery")
    TArray<float> BaseSPRecoverAmounts = { 50.f, 60.f, 70.f };

    /** 분노(State.Rage) 상태일 때 최대 생명력(MaxHP) 비례 회복 비율 (1레벨 10%, 2레벨 15%, 3레벨 20%) */
    UPROPERTY(EditDefaultsOnly, Category = "Tranquility|Recovery")
    TArray<float> RageHealHPRatios = { 0.10f, 0.15f, 0.20f };
};
