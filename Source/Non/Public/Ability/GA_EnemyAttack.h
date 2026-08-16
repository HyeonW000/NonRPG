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

    // Motion Warping에 전달할 타겟 이름 (기본값: LocationTarget)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|MotionWarping")
    FName WarpTargetName = FName("LocationTarget");

    // Motion Warping 타겟 위치를 최신 타겟 위치로 즉시 갱신하는 함수
    UFUNCTION(BlueprintCallable, Category = "Config|MotionWarping")
    void UpdateWarpTargetLocation();

    // 준비 동작 동안 실시간 유도 타겟 추적 지속 시간 (초 단위, 기본 0.5초)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|MotionWarping")
    float DynamicWarpTrackingDuration = 0.5f;

private:
    FTimerHandle DynamicWarpTrackingTimerHandle;
    void OnDynamicWarpTick();

protected:
	UFUNCTION()
	virtual void OnMontageEnded();

	UFUNCTION()
	virtual void OnMontageCancelled();
};
