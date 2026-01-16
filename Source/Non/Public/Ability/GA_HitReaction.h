#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_HitReaction.generated.h"

/**
 * 피격 리액션 어빌리티
 *  - Trigger: Effect.Hit.*
 *  - Logic: 태그에 따라 다른 몽타주 재생 + 넉백(Launch)
 */
UCLASS()
class NON_API UGA_HitReaction : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UGA_HitReaction();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, 
                                 const FGameplayAbilityActorInfo* ActorInfo, 
                                 const FGameplayAbilityActivationInfo ActivationInfo, 
                                 const FGameplayEventData* TriggerEventData) override;

protected:
    /** 태그별 몽타주 선택 (BP에서 설정 가능하도록) */
    UPROPERTY(EditDefaultsOnly, Category = "HitReact")
    TMap<FGameplayTag, UAnimMontage*> HitMontages;

    /** 넉백 강도 (태그별 설정 가능하겠지만 일단 단순화) */
    UPROPERTY(EditDefaultsOnly, Category = "HitReact")
    float DefaultKnockbackStrength = 500.f;

    /** 수직 넉백 강도 */
    UPROPERTY(EditDefaultsOnly, Category = "HitReact")
    float DefaultKnockbackUpward = 200.f;

    // Helper: 몽타주 끝날 때까지 대기
    UFUNCTION()
    void OnMontageEnded();
};
