#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "NonAbilitySystemComponent.generated.h"

UCLASS()
class NON_API UNonAbilitySystemComponent : public UAbilitySystemComponent
{
    GENERATED_BODY()

public:
    UNonAbilitySystemComponent();

    // 향후 커스텀 함수 여기에 추가 가능
};
