#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Revive.generated.h"

/**
 * 캐릭터 부활 처리를 담당하는 어빌리티
 * - 부활 애니메이션 재생
 * - 애니메이션 종료 시 어빌리티 종료
 */
UCLASS()
class NON_API UGA_Revive : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_Revive();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive")
	TObjectPtr<UAnimMontage> InPlaceReviveMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Revive")
	TObjectPtr<UAnimMontage> NearReviveMontage;

	UFUNCTION()
	void OnMontageEnded();
};
