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
    /** 이 어빌리티 전용으로 재생할 애니메이션 몽타주 (지정 시 캐릭터/DT 기본 몽타주 대신 이 몽타주를 최우선 재생!) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    TObjectPtr<class UAnimMontage> CustomMontage = nullptr;

    // Motion Warping에 전달할 타겟 이름 (기본값: LocationTarget)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|MotionWarping")
    FName WarpTargetName = FName("LocationTarget");

    // 모션 워핑 시 타겟 유저 위치로 날아갈지, 아니면 보스 전방 고정 거리로 날아갈지 여부 (기본값: false = 전방 고정 거리 점프)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|MotionWarping")
    bool bWarpToTargetActor = false;

    // 보스 전방 고정 점프 거리 (cm 단위, 기본값: 500cm = 5미터 전방 이동)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|MotionWarping")
    float JumpForwardDistance = 500.0f;

    // Motion Warping 타겟 위치를 최신 타겟 위치로 즉시 갱신하는 함수
    UFUNCTION(BlueprintCallable, Category = "Config|MotionWarping")
    void UpdateWarpTargetLocation();

    // 준비 동작 동안 실시간 유도 타겟 추적 지속 시간 (초 단위, 기본 0.5초)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|MotionWarping")
    float DynamicWarpTrackingDuration = 0.5f;

    /** 원거리 공격 모드 여부 (true면 DT_EnemyData의 RangeAttackMontage를 우선 재생) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    bool bIsRangeAttack = false;

private:
    FTimerHandle DynamicWarpTrackingTimerHandle;
    void OnDynamicWarpTick();

protected:
	UFUNCTION()
	virtual void OnMontageEnded();

	UFUNCTION()
	virtual void OnMontageCancelled();
};

/**
 * 일반 몬스터 원거리 공격 어빌리티
 * - DT_EnemyData의 RangeAttackMontage를 자동 재생
 * - 기본 태그: Enemy.Skill.RangeAttack (BTTask_EnemySkill과 완전 연동)
 */
UCLASS()
class NON_API UGA_Enemy_RangeAttack : public UGA_EnemyAttack
{
    GENERATED_BODY()
public:
    UGA_Enemy_RangeAttack();
};
