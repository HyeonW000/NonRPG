#include "AI/BTTask_TryAttack.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Character/EnemyCharacter.h"

UBTTask_TryAttack::UBTTask_TryAttack()
{
    NodeName = TEXT("Try Attack (C++)");
}

EBTNodeResult::Type UBTTask_TryAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!AIC || !BB) 
    {

        return EBTNodeResult::Failed;
    }

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIC->GetPawn());
    if (!Enemy)
    {

        return EBTNodeResult::Failed;
    }

    // 키 이름이 맞는지 확인 필요. (기본값 TargetActor)
    AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
    if (!Target)
    {

        return EBTNodeResult::Failed;
    }

    if (!Enemy->IsAttackAllowed())
    {
        // HitReact 중이거나 죽었으면 실패
        return EBTNodeResult::Failed;
    }

    const float Dist = FVector::Dist(Enemy->GetActorLocation(), Target->GetActorLocation());
    const float Range = Enemy->AttackRange;

    // 디버깅용 로그 (너무 자주 뜨면 주석 처리)


    if (Dist <= Range)
    {
        // 사정거리 첫 진입 시각 기록
        if (Enemy->EnterRangeTime < 0.f)
        {
            Enemy->MarkEnteredAttackRange();

            return EBTNodeResult::Failed; // 이번 틱엔 공격 보류
        }

        // 웜업 체크
        if (!Enemy->IsFirstAttackWindupDone())
        {
            return EBTNodeResult::Failed;
        }

        // 공격 시도

        Enemy->TryStartAttack();
        return EBTNodeResult::Succeeded;
    }
    else
    {
        // 사정거리 밖
        if (Enemy->EnterRangeTime >= 0.f)
        {
             // 범위를 벗어나면 타이머 리셋
             Enemy->EnterRangeTime = -1.f;

        }
        return EBTNodeResult::Failed;
    }
}
