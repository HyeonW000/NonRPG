#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_EnemyAttack.generated.h"

/**
 * 적 기본 공격 어빌리티
 * - EnemyAnimSet의 AttackMontages 중 하나를 랜덤 재생
 * - 어빌리티 태그: Ability.Attack
 * - 상태 태그: State.Attacking (활성화 중 부여)
 */
UCLASS()
class NON_API UGA_EnemyAttack : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_EnemyAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
    TArray<TObjectPtr<UAnimMontage>> AttackMontages;

protected:
	UFUNCTION()
	void OnMontageEnded();

	UFUNCTION()
	void OnMontageCancelled();
};
