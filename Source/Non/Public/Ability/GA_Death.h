#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Death.generated.h"

/**
 * 캐릭터 사망 처리를 담당하는 어빌리티
 * - 태그 부여: State.Dead
 * - 다른 어빌리티 취소
 * - 사망 애니메이션 재생 or 래그돌 처리
 * - 캐릭터의 물리/AI 정리 함수 호출
 */
UCLASS()
class NON_API UGA_Death : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_Death();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death")
	bool bUseRagdoll = false;

	UFUNCTION()
	void OnMontageEnded();
};
