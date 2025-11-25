// GA_SkillBase.h

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_SkillBase.generated.h"

UCLASS()
class NON_API UGA_SkillBase : public UGameplayAbility
{
    GENERATED_BODY()

protected:
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    /** 몽타주 정상 종료 콜백 */
    UFUNCTION()
    void OnMontageFinished();

    /** 몽타주 취소/중단 콜백 */
    UFUNCTION()
    void OnMontageCancelled();
};
