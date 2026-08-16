#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_EnemyTurn.generated.h"

class UAnimMontage;

/**
 * 몬스터/보스 공용 제자리 회전 어빌리티 (GA_EnemyTurn)
 * - Motion Warping 기술과 연동하여 플레이어를 향해 정교하게 회전합니다.
 * - AbilityTags: State.Turn
 * - CancelAbilitiesWithTag: State.Pain, State.Broken (부위 파괴 시 자동 취소)
 */
UCLASS()
class NON_API UGA_EnemyTurn : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_EnemyTurn();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // 회전 몽타주 설정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn|Montage")
    TObjectPtr<UAnimMontage> TurnLeftMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn|Montage")
    TObjectPtr<UAnimMontage> TurnRightMontage;

    // Motion Warping에 등록할 Target Name (기본값: RotateTarget)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn|MotionWarping")
    FName WarpTargetName = FName("RotateTarget");

private:
    UFUNCTION()
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
