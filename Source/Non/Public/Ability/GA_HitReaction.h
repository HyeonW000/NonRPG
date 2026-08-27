#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_HitReaction.generated.h"

/**
 * 피격 리액션 어빌리티
 *  - Trigger: Effect.Hit.*
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
	
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, 
                            const FGameplayAbilityActorInfo* ActorInfo, 
                            const FGameplayAbilityActivationInfo ActivationInfo, 
                            bool bReplicateEndAbility, bool bWasCancelled) override;

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual bool ShouldActivateAbility(ENetRole Role) const override;
protected:

    /** 피격/스턴 종료 후, 즉시 반격하지 않고 대기할 시간 (초) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact", meta = (ClampMin = "0.0"))
    float PostReactionAttackDelay = 0.5f;

    // 공격자가 보낸 커스텀 스턴(딜레이) 시간을 저장하는 변수
    float ActualReactionDelay = 0.f;

    // Helper: 몽타주 끝날 때까지 대기
    UFUNCTION()
    void OnMontageEnded();

    // [New] State.HitReacting 태그 개수 변동 시 콜백
    void OnStunTagChanged(const FGameplayTag Tag, int32 NewCount);

    FDelegateHandle TagEventHandle;
};
