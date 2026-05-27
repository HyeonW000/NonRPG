#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_ConsumePotion.generated.h"

UCLASS()
class NON_API UGA_ConsumePotion : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_ConsumePotion();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

protected:
    // === [New] 유연한 물약 복용 에디터 설정 (하드코딩 배제) ===
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
    float PotionDuration = 10.0f; // 기본 10초 복용

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
    float PotionTickInterval = 1.0f; // 기본 1초 주기 회복

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
    float PotionHealPercentPerTick = 0.1f; // 기본 초당 MaxHP의 10% 회복

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
    TSubclassOf<class UGameplayEffect> CooldownEffectClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
    class UAnimMontage* PotionDrinkMontage;

private:
    void CheckMovement();
    void ApplyHealTick();

    FTimerHandle MovementCheckTimerHandle;
    FTimerHandle HealTimerHandle;
};
