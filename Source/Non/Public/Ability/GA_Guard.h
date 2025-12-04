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
    /** 가드 진입용 몽타주 (선택 사항) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Anim")
    TObjectPtr<UAnimMontage> GuardEnterMontage = nullptr;

    /** 가드 해제용 몽타주 (사용 안 하면 비워둬도 됨) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Anim")
    TObjectPtr<UAnimMontage> GuardExitMontage = nullptr;
};
