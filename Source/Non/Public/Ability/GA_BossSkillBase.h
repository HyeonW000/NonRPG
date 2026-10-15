#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_BossSkillBase.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

/**
 * 보스 전용 스킬/공격 기본 어빌리티 (C++)
 * - 별도의 GE_Cooldown 블루프린트를 만들 필요 없이, CooldownDuration(초) 숫자만 입력하면 C++이 자동으로 쿨다운 적용
 * - 디테일 창에서 전용 MontageToPlay만 지정하면 즉시 동작
 * - 모션 워핑 지원
 */
UCLASS()
class NON_API UGA_BossSkillBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BossSkillBase();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// GAS 표준 쿨다운 태그 및 시간 검사 오버라이드
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	// ── 애니메이션 설정 ──
	/** 이 보스 스킬에서 재생할 전용 몽타주 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossSkill|Animation")
	TObjectPtr<UAnimMontage> MontageToPlay = nullptr;

	/** 재생 속도 배율 (기본 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossSkill|Animation")
	float PlayRate = 1.0f;

	// ── 쿨다운 설정 ──
	/** 스킬 쿨타임 (초 단위). GE 파일을 만들 필요 없이 이 숫자만 넣으면 자동으로 쿨다운 적용! */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossSkill|Cooldown", meta = (ClampMin = "0.0"))
	float CooldownDuration = 0.0f;

	/** 이 스킬의 쿨다운을 식별할 고유 태그 (예: Ability.Cooldown.Golem.Smash) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossSkill|Cooldown")
	FGameplayTag CooldownTag;

	// ── 모션 워핑 ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BossSkill|MotionWarping")
	FName WarpTargetName = FName("LocationTarget");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossSkill|MotionWarping")
	bool bWarpToTargetActor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossSkill|MotionWarping")
	float JumpForwardDistance = 500.0f;

	UFUNCTION(BlueprintCallable, Category = "BossSkill|MotionWarping")
	void UpdateWarpTargetLocation();

private:
	mutable FGameplayTagContainer TempCooldownTags;

	UFUNCTION()
	void OnMontageEnded();

	UFUNCTION()
	void OnMontageCancelled();
};
