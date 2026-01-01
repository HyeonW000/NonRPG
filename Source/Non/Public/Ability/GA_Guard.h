#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Guard.generated.h"

class ANonCharacterBase;

/**
 * 가드 유지형 GA
 * - Activate 시 캐릭터 StartGuard()
 * - Cancel/End 시 StopGuard()
 * - 입력: Press → TryActivateAbilitiesByTag(Ability.Guard)
 *         Release → CancelAbilitiesWithTag(Ability.Guard)
 */
UCLASS()
class NON_API UGA_Guard : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Guard();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

protected:

};
