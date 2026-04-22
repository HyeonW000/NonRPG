#include "AI/BTTask_FindRandomLocation.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"

UBTTask_FindRandomLocation::UBTTask_FindRandomLocation()
{
    NodeName = "Find Random Location";
}

EBTNodeResult::Type UBTTask_FindRandomLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!AIC || !BB) return EBTNodeResult::Failed;

    APawn* SelfPawn = AIC->GetPawn();
    if (!SelfPawn) return EBTNodeResult::Failed;

    UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!NavSystem) return EBTNodeResult::Failed;

    // 1. 기준점 가져오기 (HomeLocation)
    FVector CenterLocation = FVector::ZeroVector;
    if (!CenterLocationKey.IsNone())
    {
        CenterLocation = BB->GetValueAsVector(CenterLocationKey.SelectedKeyName);
    }
    else
    {
        // 기준점이 없으면 본인 위치 사용
        CenterLocation = SelfPawn->GetActorLocation();
    }

    // 2. 랜덤 지점 찾기
    FNavLocation ResultLocation;
    if (NavSystem->GetRandomReachablePointInRadius(CenterLocation, SearchRadius, ResultLocation))
    {
        // 3. 찾은 지점을 블랙보드에 저장
        BB->SetValueAsVector(ResultLocationKey.SelectedKeyName, ResultLocation.Location);
        return EBTNodeResult::Succeeded;
    }

    return EBTNodeResult::Failed;
}
