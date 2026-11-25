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

    // 잡힌 동안(State.Grabbed)에는 잡힘 연출, 사망, 부활 어빌리티만 발동 허용.
    // 공격, 회피, 가드, 스킬, 물약 등은 BP마다 차단 태그를 넣지 않아도 여기서 한 번에 막힘 (서버 검증 포함)
    virtual bool AreAbilityTagsBlocked(const FGameplayTagContainer& Tags) const override;
};
