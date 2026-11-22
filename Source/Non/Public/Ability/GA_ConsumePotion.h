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
    // === 물약 복용 설정 (설계서 11장: 비전투 시 3초 복용 HoT, 이동 시 취소) ===
    // 총 회복량은 아이템 데이터(Consumable.EffectValue)에서 이벤트로 전달받음:
    //   1 이하 = 최대 HP 대비 비율(0.3 = 30%), 1 초과 = 고정 회복량(HP)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
    float PotionDuration = 3.0f; // 복용 시간(초)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
    float PotionTickInterval = 0.5f; // 회복 주기(초)

    // 아이템 EffectValue가 0일 때 쓰는 기본 총 회복 비율 (최대 HP 대비)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
    float DefaultTotalHealRatio = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
    TSubclassOf<class UGameplayEffect> CooldownEffectClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion")
    class UAnimMontage* PotionDrinkMontage;

private:
    void CheckMovement();
    void ApplyHealTick();

    FTimerHandle MovementCheckTimerHandle;
    FTimerHandle HealTimerHandle;
    FTimerHandle EndTimerHandle;

    // 이번 복용의 틱당 회복량 (활성화 시 계산)
    float HealPerTick = 0.f;
};
