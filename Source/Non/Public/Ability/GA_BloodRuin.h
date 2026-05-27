#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_BloodRuin.generated.h"

UCLASS()
class NON_API UGA_BloodRuin : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_BloodRuin();

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
    // === [New] 유연한 요구 태그 에디터 설정 (하드코딩 배제) ===
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
    FGameplayTag RequiredStateTag;
};
