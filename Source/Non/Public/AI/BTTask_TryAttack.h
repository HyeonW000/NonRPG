#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_TryAttack.generated.h"

UCLASS(meta = (DisplayName = "TryAttack (C++)"))
class NON_API UBTTask_TryAttack : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_TryAttack();

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetActorKey; // Object(Actor)

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
