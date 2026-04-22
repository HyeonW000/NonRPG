#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindRandomLocation.generated.h"

/**
 * 특정 지점 주변의 무작위 위치를 찾아 블랙보드에 저장하는 AI 태스크
 */
UCLASS()
class NON_API UBTTask_FindRandomLocation : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_FindRandomLocation();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    // 배회할 반경 (에디터에서 수정 가능)
    UPROPERTY(EditAnywhere, Category = "Wander")
    float SearchRadius = 1000.f;

    // 중심점이 되는 위치 블랙보드 키 (예: HomeLocation)
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector CenterLocationKey;

    // 찾은 위치를 저장할 블랙보드 키 (예: PatrolLocation)
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector ResultLocationKey;
};
