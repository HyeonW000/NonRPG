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

    /** 정면 공격 허용 각도 (기본 60도: 정면 좌우 60도 부채꼴 범위 안일 때만 공격, 등 뒤에 있으면 자연스럽게 회전) */
    UPROPERTY(EditAnywhere, Category = "Attack", meta = (ClampMin = "10.0", ClampMax = "180.0"))
    float MaxFacingAngle = 60.0f;

    /** 제자리 회전 속도 (도/초, 기본 360도/초: 등 뒤에 있는 플레이어를 향해 부드럽게 도는 속도) */
    UPROPERTY(EditAnywhere, Category = "Attack", meta = (ClampMin = "60.0", ClampMax = "1080.0"))
    float TurnSpeed = 360.0f;

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
