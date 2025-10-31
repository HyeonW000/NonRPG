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

    UE_LOG(LogTemp, Warning, TEXT("[NonAIController] OnPossess: %s"), *GetNameSafe(InPawn));

    if (!BehaviorTreeAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("[NonAIController] BehaviorTreeAsset is NULL! Set it on BP_NonAIController."));
        return;
    }

    // Blackboard 사용
    if (!UseBlackboard(BehaviorTreeAsset->BlackboardAsset, BBComp))
    {
        UE_LOG(LogTemp, Error, TEXT("[NonAIController] UseBlackboard FAILED."));
        return;
    }

    // BT 실행
    const bool bRunOk = RunBehaviorTree(BehaviorTreeAsset);
    UE_LOG(LogTemp, Warning, TEXT("[NonAIController] RunBehaviorTree: %s"), bRunOk ? TEXT("OK") : TEXT("FAIL"));

    if (!bRunOk)
    {
        // 실패 시에도 한 번 더 로그
        if (BehaviorTreeAsset->BlackboardAsset == nullptr)
        {
            UE_LOG(LogTemp, Error, TEXT("[NonAIController] BT has NO BlackboardAsset assigned."));
        }
    }
}

void ANonAIController::OnUnPossess()
{
    Super::OnUnPossess();
    UE_LOG(LogTemp, Warning, TEXT("[NonAIController] OnUnPossess"));
}
