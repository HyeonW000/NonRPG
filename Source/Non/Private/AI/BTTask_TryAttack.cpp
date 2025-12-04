#include "AI/BTTask_TryAttack.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "AI/EnemyCharacter.h"

UBTTask_TryAttack::UBTTask_TryAttack()
{
    NodeName = TEXT("Try Attack (C++)");
}

EBTNodeResult::Type UBTTask_TryAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!AIC || !BB) return EBTNodeResult::Failed;

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIC->GetPawn());
    AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
    if (!Enemy || !Target) return EBTNodeResult::Failed;

    if (!Enemy->IsAttackAllowed())
    {
        return EBTNodeResult::Failed;
    }

    const float Dist = FVector::Dist(Enemy->GetActorLocation(), Target->GetActorLocation());
    if (Dist <= Enemy->AttackRange)
    {
        // 사정거리 첫 진입 시각 기록 (아직 기록 없으면 지금 기록하고 실패 반환)
        if (Enemy->EnterRangeTime < 0.f)
        {
            Enemy->MarkEnteredAttackRange();
            return EBTNodeResult::Failed; // 이번 틱엔 공격 보류
        }

        // 웜업이 아직 끝나지 않았다면 계속 보류
        if (!Enemy->IsFirstAttackWindupDone())
        {
            return EBTNodeResult::Failed;
        }

        // 여기까지 왔으면 웜업 종료 + 공격 가능
        Enemy->TryStartAttack();
        return EBTNodeResult::Succeeded;
    }

    // 사정거리 밖: 기록 초기화
    Enemy->EnterRangeTime = -1.f;
    return EBTNodeResult::Failed;
}
