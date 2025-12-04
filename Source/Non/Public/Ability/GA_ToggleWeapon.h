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


    /** Draw 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToggleWeapon|Montage")
    TObjectPtr<UAnimMontage> DrawMontage = nullptr;

    /** Sheathe 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToggleWeapon|Montage")
    TObjectPtr<UAnimMontage> SheatheMontage = nullptr;

    /** 현재 재생 중인 몽타주(EndDelegate용) */
    UPROPERTY(Transient)
    TObjectPtr<UAnimMontage> ActiveMontage = nullptr;

    //  이번 토글에서 목표 무장 상태 (true = 무장, false = 해제)
    UPROPERTY(Transient)
    bool bTargetArmed = false;
    // 무장 상태 변경할 캐릭터 (소유자 캐시)
    UPROPERTY(Transient)
    TWeakObjectPtr<ANonCharacterBase> CachedNonChar;

protected:
    /** 몽타주 끝났을 때 GA 종료 */
    UFUNCTION()
    void OnToggleMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
