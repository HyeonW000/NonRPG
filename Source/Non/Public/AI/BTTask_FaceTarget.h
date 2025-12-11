#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FaceTarget.generated.h"

UCLASS()
class NON_API UBTTask_FaceTarget : public UBTTaskNode
{
    GENERATED_BODY()
    
public:
    UBTTask_FaceTarget();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    struct FBlackboardKeySelector TargetKey;

    UPROPERTY(EditAnywhere, Category = "Config")
    float Precision = 5.0f; // 5도 이내면 성공 간주
};
