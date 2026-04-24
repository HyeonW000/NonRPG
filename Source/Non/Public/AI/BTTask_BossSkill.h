#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"
#include "BTTask_BossSkill.generated.h"

UCLASS()
class NON_API UBTTask_BossSkill : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_BossSkill();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    // 특정 태그를 가진 스킬만 골라서 사용하고 싶을 때 설정 (비어있으면 모든 스킬 대상)
    UPROPERTY(EditAnywhere, Category = "BossAI")
    FGameplayTag CategoryTag;

    // 여러 스킬 중 무작위로 고를지, 아니면 순번대로 고를지 (이 블루프린트 노드를 커스텀할 수도 있음)
    UPROPERTY(EditAnywhere, Category = "BossAI")
    bool bRandomSkillSelection = true;

    // 적용될 최소 거리 (이보다 가까우면 실패)
    UPROPERTY(EditAnywhere, Category = "BossAI")
    float MinDistance = 0.f;

    // 적용될 최대 거리 (이보다 멀면 실패)
    UPROPERTY(EditAnywhere, Category = "BossAI")
    float MaxDistance = 99999.f;

    // 실행 확률 (0.0 ~ 1.0)
    UPROPERTY(EditAnywhere, Category = "BossAI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ActivateProbability = 1.0f;

    // 거리 계산에 사용할 타격 대상 블랙보드 키
    UPROPERTY(EditAnywhere, Category = "BossAI")
    FBlackboardKeySelector TargetActorKey;
};
