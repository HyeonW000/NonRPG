#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_ComboBase.generated.h"

UCLASS()
class NON_API UGA_ComboBase : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UGA_ComboBase();

    // 콤보 입력 허용 여부
    void SetComboWindowOpen(bool bOpen);

    // 콤보 입력을 버퍼에 저장
    void BufferComboInput();

    // 원본 네이밍에 맞춤: bBufferedComboInput 사용
    bool WasComboInputBuffered() const { return bBufferedComboInput; }

protected:
    // 호환성 유지용(더 이상 사용하지 않음). AnimBP 데이터에셋 경유로 대체됨.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo", meta = (DeprecatedProperty, DisplayName = "(Deprecated) ComboMontage (unused)"))
    TObjectPtr<class UAnimMontage> ComboMontage;

    // 어빌리티 실행
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    // 어빌리티 종료
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

    // 애니메이션 끝났을 때 콜백
    UFUNCTION()
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // 다음 콤보 어빌리티 실행
    void TryActivateNextCombo();

protected:
    // 현재 실제로 재생 중인 콤보 몽타주(AnimBP에서 받은 것)
    UPROPERTY(Transient)
    TObjectPtr<class UAnimMontage> ActiveMontage = nullptr;

    // 콤보 입력 수용 상태
    bool bComboWindowOpen = false;

    // 입력이 버퍼되었는가? 
    bool bBufferedComboInput = false;

    // 노티파이 끝에서 체인 트리거가 이미 걸렸는지 보호용
    bool bChainRequestedAtWindowEnd = false;

    UPROPERTY()
    TObjectPtr<class ACharacter> Character;

    UPROPERTY()
    TObjectPtr<class UAbilitySystemComponent> ASC;
};
