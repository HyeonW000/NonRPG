#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_ToggleWeapon.generated.h"

class ANonCharacterBase;

/**
 * 무기 Draw / Sheathe 토글용 GA
 * - 현재 무장 상태를 보고 발도 또는 납도 몽타주 하나를 끝까지 재생 (몽타주 교체 없음)
 * - 제자리에서는 루트 이동(왼발 고정, 무게중심 이동) 그대로, 움직이기 시작하면 그 몽타주의 루트 이동만 끔
 * - 하체는 애님 BP가 LocomotionLowerBodyAlpha로 이동 애니메이션과 섞음 (DrawSheatheSlot)
 * - 실제 무기 소켓 이동은 몽타주의 AnimNotify_SwapWeaponSocket, 누락 시 종료 때 ApplyTargetWeaponState가 보장
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

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

protected:
    /** 발도 몽타주 (제자리 루트 이동 포함, 슬롯 DrawSheatheSlot). 이름은 BP 값 유지를 위해 _Root 그대로 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToggleWeapon|Montage")
    TObjectPtr<UAnimMontage> DrawMontage_Root = nullptr;

    /** 납도 몽타주 (제자리 루트 이동 포함, 슬롯 DrawSheatheSlot) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToggleWeapon|Montage")
    TObjectPtr<UAnimMontage> SheatheMontage_Root = nullptr;

    /** 현재 재생 중인 몽타주 */
    UPROPERTY(Transient)
    TObjectPtr<UAnimMontage> ActiveMontage = nullptr;

    // 이번 토글에서 목표 무장 상태 (true = 무장, false = 해제)
    UPROPERTY(Transient)
    bool bTargetArmed = false;

    // 이번 재생에서 이동 때문에 루트 이동을 껐는지
    UPROPERTY(Transient)
    bool bRootMotionDisabledForMove = false;

    // 무장 상태 변경할 캐릭터 (소유자 캐시)
    UPROPERTY(Transient)
    TWeakObjectPtr<ANonCharacterBase> CachedNonChar;

    // 몽타주 재생을 제어하는 GAS AbilityTask
    UPROPERTY(Transient)
    TObjectPtr<class UAbilityTask_PlayMontageAndWait> PlayMontageTask = nullptr;

    // 재생 중 이동 시작을 감지하는 타이머
    FTimerHandle MovementCheckTimerHandle;

    /** 몽타주 정상 완료 콜백 */
    UFUNCTION()
    void OnMontageFinished();

    /** 몽타주 강제 중단/취소 콜백 */
    UFUNCTION()
    void OnMontageInterrupted();

    /** 노티파이 누락 시에도 무장 상태·소켓·태그를 보장 */
    void ApplyTargetWeaponState();

    /** 이동 시작 감지 (움직이면 루트 이동 끔) */
    void CheckCharacterMovement();

    /** 이동 시작 시 재생 중인 몽타주의 루트 이동만 끄고 회전 잠금 해제 */
    void DisableRootMotionForMove();

    /** 타이머 해제 */
    void ClearMovementTimer();
};
