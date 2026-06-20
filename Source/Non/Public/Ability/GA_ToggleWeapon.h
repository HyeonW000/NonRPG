#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_ToggleWeapon.generated.h"

class ANonCharacterBase;

/**
 * 무기 Draw / Sheathe 토글용 GA
 * - 현재 무장 상태를 보고 Draw or Sheathe 선택
 * - 정지 / 이동 여부에 따라 FullBody / UpperBody 몽타주 선택
 * - 실제 무기 장착/해제, 소켓 변경 등은 AnimNotify 또는 캐릭터 블루프린트에서 처리
 */
UCLASS()
class NON_API UGA_ToggleWeapon : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_ToggleWeapon();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;



protected:


    /** Draw 몽타주 (루트 모션 전신) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToggleWeapon|Montage")
    TObjectPtr<UAnimMontage> DrawMontage_Root = nullptr;

    /** Draw 몽타주 (일반 상체) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToggleWeapon|Montage")
    TObjectPtr<UAnimMontage> DrawMontage_NoRoot = nullptr;

    /** Sheathe 몽타주 (루트 모션 전신) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToggleWeapon|Montage")
    TObjectPtr<UAnimMontage> SheatheMontage_Root = nullptr;

    /** Sheathe 몽타주 (일반 상체) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToggleWeapon|Montage")
    TObjectPtr<UAnimMontage> SheatheMontage_NoRoot = nullptr;

    /** 현재 재생 중인 몽타주(EndDelegate용) */
    UPROPERTY(Transient)
    TObjectPtr<UAnimMontage> ActiveMontage = nullptr;

    // 이번 토글에서 목표 무장 상태 (true = 무장, false = 해제)
    UPROPERTY(Transient)
    bool bTargetArmed = false;

    // 무장 상태 변경할 캐릭터 (소유자 캐시)
    UPROPERTY(Transient)
    TWeakObjectPtr<ANonCharacterBase> CachedNonChar;

    // 몽타주 재생을 제어하는 GAS AbilityTask
    UPROPERTY(Transient)
    TObjectPtr<class UAbilityTask_PlayMontageAndWait> PlayMontageTask = nullptr;

    // 가만히 있을 때 재생을 시작한 후 이동을 감지하기 위한 타이머 핸들
    FTimerHandle MovementCheckTimerHandle;

public:
    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

protected:
    /** 몽타주 종료 콜백 (AbilityTask용) */
    UFUNCTION()
    void OnMontageFinished();

    /** 캐릭터 이동을 매 주기 체크하는 함수 */
    void CheckCharacterMovement();

    /** 몽타주 버전을 실시간으로 양방향 스왑하는 함수 */
    void SwitchMontage(bool bPlayNoRoot);

    /** 타이머 해제 및 정리 함수 */
    void ClearMovementTimer();
};

