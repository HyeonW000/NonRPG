#include "AI/NonAIController.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/EnemyCharacter.h"
#include "Character/BossCharacter.h"
#include "Kismet/GameplayStatics.h"

ANonAIController::ANonAIController()
{
    // 필요 시 Perception, PathFollowing 세팅 추가 가능
}

void ANonAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    UBehaviorTree* TargetBT = BehaviorTreeAsset;

    // 1. 캐릭터 자체에 지정된 커스텀 BT가 있다면 최우선 실행
    if (AEnemyCharacter* EnemyChar = Cast<AEnemyCharacter>(InPawn))
    {
        if (EnemyChar->CustomBehaviorTree)
        {
            TargetBT = EnemyChar->CustomBehaviorTree;
        }
        else if (EnemyChar->IsBoss() && BossBehaviorTreeAsset)
        {
            TargetBT = BossBehaviorTreeAsset;
        }
    }
    // 2. ABossCharacter인 경우 보스 BT 우선 선택
    else if (InPawn && InPawn->IsA<ABossCharacter>())
    {
        if (BossBehaviorTreeAsset)
        {
            TargetBT = BossBehaviorTreeAsset;
        }
    }

    if (!TargetBT)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ [ANonAIController] '%s' 에 실행할 비헤이비어 트리가 설정되어 있지 않습니다! (BehaviorTreeAsset/BossBehaviorTreeAsset 확인 필요)"), *GetNameSafe(InPawn));
        return;
    }

    // Blackboard 사용
    if (!UseBlackboard(TargetBT->BlackboardAsset, BBComp))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ [ANonAIController] '%s'의 블랙보드 초기화 실패!"), *GetNameSafe(TargetBT));
        return;
    }

    // 처음 소환된 위치를 HomeLocation으로 기록
    if (BBComp && InPawn)
    {
        BBComp->SetValueAsVector(TEXT("HomeLocation"), InPawn->GetActorLocation());
    }

    // BT 실행
    const bool bRunOk = RunBehaviorTree(TargetBT);
    if (!bRunOk)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [ANonAIController] RunBehaviorTree 실패: '%s'"), *GetNameSafe(TargetBT));
    }
}

void ANonAIController::OnUnPossess()
{
    Super::OnUnPossess();
}

FPathFollowingRequestResult ANonAIController::MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath)
{
    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetPawn()))
    {
        // ★ 1. 현재 후퇴(카이팅) 이동 중인 경우:
        // TryPerformBackstepOrFlee 내부에서 발송한 후퇴 이동(bIsIssuingFleeMove)만 Super::MoveTo로 통과시키고,
        // BT의 다른 모든 이동 요청(플레이어 추적 MoveToActor, 포위 슬롯 MoveToLocation 등)은 후퇴가 끝날 때까지 차단!
        if (Enemy->IsFleeing())
        {
            if (Enemy->bIsIssuingFleeMove)
            {
                return Super::MoveTo(MoveRequest, OutPath);
            }
            FPathFollowingRequestResult Result;
            Result.Code = EPathFollowingRequestResult::AlreadyAtGoal;
            return Result;
        }

        // ★ 2. 사망/복귀 상태가 아닐 때 원거리 및 하이브리드 몬스터의 불필요한 근접 접근 원천 차단 (보스는 자체 BT와 스킬 사정거리를 따르므로 제외!)
        if (!Enemy->IsDead() && !Enemy->bIsLeashingHome && !Enemy->IsBoss())
        {
            const bool bIsPureRanged = (Enemy->CombatStyle == EEnemyCombatStyle::Ranged) || (Enemy->AttackMontages.Num() == 0);
            const bool bIsRangedOrHybrid = bIsPureRanged ||
                                           (Enemy->CombatStyle == EEnemyCombatStyle::Hybrid) ||
                                           (Enemy->RangeAttackMontage != nullptr);

            if (bIsRangedOrHybrid)
            {
                AActor* GoalActor = MoveRequest.IsMoveToActorRequest() ? MoveRequest.GetGoalActor() : nullptr;

                // 만약 MoveToLocation(예: TargetLocation 포위 슬롯 좌표) 요청이더라도,
                // 그 목표 좌표가 현재 타겟 플레이어 주변(480cm 이내)이라면 플레이어에게 걸어들어가려는 요청으로 판별!
                if (!GoalActor && BBComp)
                {
                    if (AActor* BBTarget = Cast<AActor>(BBComp->GetValueAsObject(TEXT("TargetActor"))))
                    {
                        const float GoalToTargetDist = FVector::Dist2D(MoveRequest.GetGoalLocation(), BBTarget->GetActorLocation());
                        if (GoalToTargetDist <= 480.0f)
                        {
                            GoalActor = BBTarget;
                        }
                    }
                }

                if (GoalActor)
                {
                    const float DistToGoal = FVector::Dist2D(Enemy->GetActorLocation(), GoalActor->GetActorLocation());
                    const float MaxRange = (Enemy->RangeAttackRange > 0.f) ? Enemy->RangeAttackRange : 1200.f;
                    const float MeleeEngageDist = (Enemy->FleeTriggerDistance > 0.f) ? Enemy->FleeTriggerDistance : 350.0f;
                    const bool bHybridInMeleeMode = !bIsPureRanged && (!Enemy->bPendingHybridFlee) && (DistToGoal <= MeleeEngageDist || Enemy->HybridMeleeAttackCount > 0);

                    // 이미 원거리 사정거리(MaxRange) 이내에 있는 경우:
                    if (DistToGoal <= MaxRange)
                    {
                        // 1) 순수 원거리 몬스터의 위험 거리 진입 시, 또는 하이브리드 몬스터가 목표 근접 공격 횟수를 다 마쳤을 때만 후퇴 시도
                        if (Enemy->bFleeWhenTooClose && ((bIsPureRanged && DistToGoal <= Enemy->FleeTriggerDistance) || (!bIsPureRanged && Enemy->bPendingHybridFlee)))
                        {
                            if (Enemy->TryPerformBackstepOrFlee(GoalActor))
                            {
                                FPathFollowingRequestResult Result;
                                Result.Code = EPathFollowingRequestResult::AlreadyAtGoal;
                                return Result;
                            }
                        }

                        // ★ [하이브리드 근접 돌입 허용]: 근접 교전 거리(350cm) 안으로 들어왔거나 이미 근접 콤보 중인 하이브리드는
                        // 제자리 정지하지 않고 플레이어에게 걸어붙어(MoveTo) 근접 사정거리 안으로 진입하도록 허용!
                        if (bHybridInMeleeMode)
                        {
                            return Super::MoveTo(MoveRequest, OutPath);
                        }

                        // 2) 순수 원거리 몬스터, 및 원거리 사격 모드인 하이브리드는 제자리 유지
                        StopMovement();

                        FVector DirToTarget = (GoalActor->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
                        if (!DirToTarget.IsNearlyZero())
                        {
                            FRotator Rot = DirToTarget.Rotation();
                            Rot.Pitch = 0.f;
                            Rot.Roll = 0.f;
                            const float TurnRate = (Enemy->AimTurnRate > 0.0f) ? Enemy->AimTurnRate : 220.0f;
                            FRotator NewRot = FMath::RInterpConstantTo(Enemy->GetActorRotation(), Rot, GetWorld()->GetDeltaSeconds(), TurnRate);
                            Enemy->SetActorRotation(NewRot);
                        }

                        FPathFollowingRequestResult Result;
                        Result.Code = EPathFollowingRequestResult::AlreadyAtGoal;
                        return Result;
                    }
                }
            }
        }
    }

    return Super::MoveTo(MoveRequest, OutPath);
}
