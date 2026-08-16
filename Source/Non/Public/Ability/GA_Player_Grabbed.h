#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Player_Grabbed.generated.h"

class UAnimMontage;

/**
 * 잡혔을 때 플레이어가 노드 조작 0개로 반응하는 C++ 어빌리티 클래스
 */
UCLASS()
class NON_API UGA_Player_Grabbed : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Player_Grabbed();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// 에디터 디테일 창에서 고르는 플레이어 피잡기 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Grabbed")
	TObjectPtr<UAnimMontage> GrabbedMontage;

	UFUNCTION()
	void OnMontageEnded();
};
