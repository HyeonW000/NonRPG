#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "GameplayTagContainer.h"
#include "BTDecorator_HasTag.generated.h"

/**
 * 액터가 특정 Gameplay Tag를 가지고 있는지 직접 확인하는 데코레이터
 */
UCLASS()
class NON_API UBTDecorator_HasTag : public UBTDecorator
{
    GENERATED_BODY()

public:
    UBTDecorator_HasTag();

protected:
    virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

    // 확인할 태그
    UPROPERTY(EditAnywhere, Category = "Condition")
    FGameplayTag TargetTag;

    // 조건을 반전시킬지 여부 (예: 태그가 '없을 때' 성공하게 하려면 True)
    UPROPERTY(EditAnywhere, Category = "Condition")
    bool bInvertCondition = false;
};
