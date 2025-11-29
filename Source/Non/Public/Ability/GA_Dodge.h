#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Dodge.generated.h"

UENUM(BlueprintType)
enum class EDodgeDirection : uint8
{
    Forward,
    Backward,
    Left,
    Right,
    ForwardLeft,
    ForwardRight,
    BackwardLeft,
    BackwardRight
};

UCLASS()
class NON_API UGA_Dodge : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Dodge();

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
    /* ======= Cost ======= */

    /** 스태미나 고정 소모량 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Cost")
    float StaminaCost = 20.f;

    /* ======= Anim (방향별 몽타주) ======= */

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Anim")
    TObjectPtr<UAnimMontage> ForwardMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Anim")
    TObjectPtr<UAnimMontage> BackwardMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Anim")
    TObjectPtr<UAnimMontage> LeftMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Anim")
    TObjectPtr<UAnimMontage> RightMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Anim")
    TObjectPtr<UAnimMontage> ForwardLeftMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Anim")
    TObjectPtr<UAnimMontage> ForwardRightMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Anim")
    TObjectPtr<UAnimMontage> BackwardLeftMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Anim")
    TObjectPtr<UAnimMontage> BackwardRightMontage = nullptr;

    UFUNCTION()
    void OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted);

protected:
    /** 입력(월드) → 8방향 */
    EDodgeDirection CalcDodgeDirection(const class ACharacter* Char, const FVector& WorldInput) const;

    /** 방향에 맞는 몽타주 선택 */
    class UAnimMontage* GetMontage(EDodgeDirection Dir) const;
};
