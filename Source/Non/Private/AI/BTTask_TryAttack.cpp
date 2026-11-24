#include "AI/BTTask_TryAttack.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Character/EnemyCharacter.h"

UBTTask_TryAttack::UBTTask_TryAttack()
{
    NodeName = TEXT("Try Attack (C++)");
    bNotifyTick = true;
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

    FName KeyName = TargetActorKey.SelectedKeyName.IsNone() ? FName(TEXT("TargetActor")) : TargetActorKey.SelectedKeyName;
    AActor* Target = Cast<AActor>(BB->GetValueAsObject(KeyName));
    // 블랙보드에 대상이 없으면 복제되는 추격 대상으로 대체 (첫 플레이어로 대체하면 협동에서 항상 호스트를 노림)
    if (!Target)
    {
        Target = Enemy->GetAggroTarget();
    }
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
    // DT_EnemyData의 AttackRange를 정확하게 반영 (비어있으면 기본 150cm)
    const float Range = (Enemy->AttackRange > 0.f) ? Enemy->AttackRange : 150.f;

    if (Dist <= Range)
    {
        // 사정거리 첫 진입 시각 기록
        if (Enemy->EnterRangeTime < 0.f)
        {
            Enemy->MarkEnteredAttackRange();
            return EBTNodeResult::Failed; // 이번 틱엔 웜업 시작 후 보류
        }

        // 웜업 체크 (초기 진입 후 잠깐 뜸들이는 동작)
        if (!Enemy->IsFirstAttackWindupDone())
        {
            return EBTNodeResult::Failed;
        }

        // 정면 각도 검사
        FVector Forward = Enemy->GetActorForwardVector().GetSafeNormal2D();
        FVector DirToTarget = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
        float Dot = FVector::DotProduct(Forward, DirToTarget);
        float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));

        // 플레이어가 이미 정면(MaxFacingAngle 이내)에 있으면 즉시 공격 시작!
        if (AngleDeg <= MaxFacingAngle)
        {
            if (!DirToTarget.IsNearlyZero())
            {
                FRotator TargetRot = DirToTarget.Rotation();
                TargetRot.Pitch = 0.0f;
                TargetRot.Roll = 0.0f;
                Enemy->SetActorRotation(TargetRot);
            }

            Enemy->TryStartAttack();
            return EBTNodeResult::Succeeded;
        }
        else
        {
            // 플레이어가 등 뒤/측면에 있으면 멍때리지 않고 부드럽게 제자리 회전하도록 InProgress 진입!
            return EBTNodeResult::InProgress;
        }
    }
    else
    {
        // 사정거리 밖으로 완전히 벗어났을 때만 타이머 리셋 (+80cm 버퍼)
        if (Dist > (Range + 80.f) && Enemy->EnterRangeTime >= 0.f)
        {
            Enemy->EnterRangeTime = -1.f;
        }
        return EBTNodeResult::Failed;
    }
}

void UBTTask_TryAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!AIC || !BB)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIC->GetPawn());
    if (!Enemy || !Enemy->IsAttackAllowed())
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    FName KeyName = TargetActorKey.SelectedKeyName.IsNone() ? FName(TEXT("TargetActor")) : TargetActorKey.SelectedKeyName;
    AActor* Target = Cast<AActor>(BB->GetValueAsObject(KeyName));
    // 블랙보드에 대상이 없으면 복제되는 추격 대상으로 대체 (첫 플레이어로 대체하면 협동에서 항상 호스트를 노림)
    if (!Target)
    {
        Target = Enemy->GetAggroTarget();
    }
    if (!Target)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    const float Dist = FVector::Dist(Enemy->GetActorLocation(), Target->GetActorLocation());
    const float Range = (Enemy->AttackRange > 0.f) ? Enemy->AttackRange : 150.f;

    // 회전 도중 플레이어가 사정거리 밖으로 멀어지면 추적 노드로 넘어가도록 실패 처리 (+30cm 버퍼)
    if (Dist > (Range + 30.f))
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    FVector DirToTarget = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
    if (DirToTarget.IsNearlyZero())
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    FRotator TargetRot = DirToTarget.Rotation();
    TargetRot.Pitch = 0.0f;
    TargetRot.Roll = 0.0f;

    // 설정된 TurnSpeed 속도로 부드럽게 제자리 회전!
    FRotator CurrentRot = Enemy->GetActorRotation();
    FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, TargetRot, DeltaSeconds, TurnSpeed);
    Enemy->SetActorRotation(NewRot);

    // 회전 후 각도 재검사
    FVector NewForward = Enemy->GetActorForwardVector().GetSafeNormal2D();
    float Dot = FVector::DotProduct(NewForward, DirToTarget);
    float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));

    // 정면 각도 안에 들어왔으면 즉시 공격 개시!
    if (AngleDeg <= MaxFacingAngle)
    {
        Enemy->SetActorRotation(TargetRot);
        Enemy->TryStartAttack();
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}
