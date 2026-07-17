#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_AttackPowerPercent.generated.h"

UCLASS()
class NON_API UMMC_AttackPowerPercent : public UGameplayModMagnitudeCalculation
{
    GENERATED_BODY()

public:
    UMMC_AttackPowerPercent();

    virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
    FGameplayEffectAttributeCaptureDefinition AttackPowerDef;
};
