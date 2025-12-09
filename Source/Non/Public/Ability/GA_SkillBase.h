// GA_SkillBase.h

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_SkillBase.generated.h"

UCLASS()
class NON_API UGA_SkillBase : public UGameplayAbility
{
    GENERATED_BODY()

public:
    // BP에서 가져다 쓸 수 있게 Getter
    UFUNCTION(BlueprintPure, Category = "Skill")
    float GetCurrentDamageScale() const { return CurrentDamageScale; }

    UFUNCTION(BlueprintPure, Category = "Skill")
    int32 GetCurrentSkillLevel() const { return CurrentSkillLevel; }

protected:
    // 스킬 실행
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    // 스킬 종료(몽타주 끝/취소 포함)
    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

    /** 몽타주 정상 종료 델리게이트 */
    UFUNCTION()
    void OnMontageFinished();

    /** 몽타주 취소/인터럽트 델리게이트 */
    UFUNCTION()
    void OnMontageCancelled();

    // 현재 실행 중인 스킬의 레벨/계수 캐시
    UPROPERTY()
    int32 CurrentSkillLevel = 1;

    UPROPERTY()
    float CurrentDamageScale = 1.f;
};
