#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FaceTarget.generated.h"

/**
 * 애니메이션(루트 모션) 회전이 완료될 때까지 기다리는 태스크
 * 보스를 직접 회전시키지 않고, 각도가 맞을 때까지 BT의 흐름을 대기시킵니다.
 */
UCLASS()
class NON_API UBTTask_FaceTarget : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_FaceTarget();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetKey;

    // 이 각도 안에 들어오면 정면으로 간주하고 성공 처리
    UPROPERTY(EditAnywhere, Category = "FaceTarget")
    float AcceptableAngle = 15.0f;

private:
    TWeakObjectPtr<AActor> CachedTarget;
};
