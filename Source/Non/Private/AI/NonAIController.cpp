#include "AI/NonAIController.h"
#include "BehaviorTree/BlackboardData.h"
#include "Kismet/GameplayStatics.h"

ANonAIController::ANonAIController()
{
    // 필요 시 Perception, PathFollowing 세팅 추가 가능
}

void ANonAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (!BehaviorTreeAsset)
    {
        return;
    }

    // Blackboard 사용
    if (!UseBlackboard(BehaviorTreeAsset->BlackboardAsset, BBComp))
    {
        return;
    }

    // BT 실행
    const bool bRunOk = RunBehaviorTree(BehaviorTreeAsset);
    if (!bRunOk)
    {
        // 실패 시에도 한 번 더 로그
        if (BehaviorTreeAsset->BlackboardAsset == nullptr)
        {
        }
    }
}

void ANonAIController::OnUnPossess()
{
    Super::OnUnPossess();
}
