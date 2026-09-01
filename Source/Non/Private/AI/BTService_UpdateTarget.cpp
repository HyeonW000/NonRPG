#include "AI/BTService_UpdateTarget.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "Character/EnemyCharacter.h"
#include "GameFramework/Pawn.h"
#include "Character/NonCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"

UBTService_UpdateTarget::UBTService_UpdateTarget()
{
    bNotifyTick = true;
    Interval = 0.20f;     // 감지 주기 (0.1~0.3 추천)
    RandomDeviation = 0.f;
    NodeName = TEXT("Update Target (C++)");
}

void UBTService_UpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    AAIController* AIC = OwnerComp.GetAIOwner();
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!AIC || !BB) return;

    AEnemyCharacter* Self = Cast<AEnemyCharacter>(AIC->GetPawn());
    if (!Self) return;

    if (Self->IsSpawnFading())
    {
        return;
    }

    UWorld* World = AIC->GetWorld();
    if (!World) return;
    const float Now = World->GetTimeSeconds();

    AActor* Curr = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));

    const bool bReactiveMode =
        bRespectAggroStyle ? (Self->AggroStyle == EAggroStyle::Reactive) : false;

    // ── 1) 기존 타겟 유지/해제 검사 ───────────────────────
    if (Curr)
    {
        bool bLoseTarget = false;
        const float DistToCurr = FVector::Dist2D(Self->GetActorLocation(), Curr->GetActorLocation());

        // 0) 플레이어가 죽었는지 검사
        if (ANonCharacterBase* TargetChar = Cast<ANonCharacterBase>(Curr)) {
            if (TargetChar->IsDead()) {
                bLoseTarget = true;
            }
        }

        // 1) 플레이어가 ExitRadius 밖으로 나감 + 최소 유지시간
        if (DistToCurr > ExitRadius && (Now - LastSwitchTime) >= MinHoldTimeOnExit)
        {
            bLoseTarget = true;
        }

        // 2) Reactive 모드: 맞아서 생긴 어그로가 만료되었는지
        if (bReactiveMode)
        {
            const bool bAggroFlag = Self->IsAggroByHit();
            const float TimeSinceHit = Now - Self->GetLastAggroByHitTime();
            const bool bAggroExpired = (!bAggroFlag) || (TimeSinceHit > Self->GetAggroByHitHoldTime());

            if (bAggroExpired)
            {
                bLoseTarget = true;
            }
        }

        // 3) 홈 리쉬: 스폰 지점에서 너무 멀어졌는지
        if (bUseHomeLeash)
        {
            const float DistFromHome = FVector::Dist2D(Self->GetActorLocation(), Self->SpawnLocation);
            if (DistFromHome > HomeLeashRadius)
            {
                bLoseTarget = true;
            }
        }

        if (bLoseTarget)
        {
            BB->ClearValue(TargetActorKey.SelectedKeyName);
            Self->SetAggro(false);
            LastSwitchTime = Now;
            Curr = nullptr; // 신규 타겟을 즉시 찾도록 비움
        }
        else
        {
            // 실시간 거리/각도 기록
            if (!DistanceKey.IsNone())
            {
                BB->SetValueAsFloat(DistanceKey.SelectedKeyName, DistToCurr);
            }
            if (!AngleKey.IsNone())
            {
                float AbsAngle = FMath::Abs(Self->GetAngleToTarget(Curr));
                BB->SetValueAsFloat(AngleKey.SelectedKeyName, AbsAngle);
            }

            // 타겟을 정상 유지하고 있으면 달리기 속도 유지 후 종료
            if (UCharacterMovementComponent* MoveComp = Self->GetCharacterMovement())
            {
                MoveComp->MaxWalkSpeed = (Self->CombatRunSpeed > 0.0f) ? Self->CombatRunSpeed : CombatRunSpeed;
            }
            return;
        }
    }

    // ── 2) 멀티플레이 신규 타겟 획득 (가장 가까운 살아있는 플레이어 탐색) ─────────
    if (!Curr && (Now - LastSwitchTime) >= MinHoldTimeOnEnter)
    {
        AActor* BestTarget = nullptr;
        float BestDist = EnterRadius;

        // 접속한 모든 플레이어 컨트롤러를 순회하여 가장 가까운 플레이어를 탐색!
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            if (APlayerController* PC = It->Get())
            {
                if (APawn* TargetPawn = PC->GetPawn())
                {
                    if (ANonCharacterBase* PlayerChar = Cast<ANonCharacterBase>(TargetPawn))
                    {
                        if (!PlayerChar->IsDead())
                        {
                            const float Dist = FVector::Dist2D(Self->GetActorLocation(), PlayerChar->GetActorLocation());
                            if (Dist < BestDist)
                            {
                                BestDist = Dist;
                                BestTarget = PlayerChar;
                            }
                        }
                    }
                }
            }
        }

        if (BestTarget)
        {
            bool bCanAggro = true;

            if (bReactiveMode)
            {
                const bool bAggroFlag = Self->IsAggroByHit();
                const float TimeSinceHit = Now - Self->GetLastAggroByHitTime();
                const bool bWithinHold = (TimeSinceHit <= Self->GetAggroByHitHoldTime());
                bCanAggro = (bAggroFlag && bWithinHold);
            }

            if (bCanAggro)
            {
                BB->SetValueAsObject(TargetActorKey.SelectedKeyName, BestTarget);
                LastSwitchTime = Now;

                // 어그로 시작 플래그
                Self->SetAggro(true);

                // 사정거리 진입 시각 기록 (웜업용)
                Self->MarkEnteredAttackRange();

                if (!DistanceKey.IsNone())
                {
                    BB->SetValueAsFloat(DistanceKey.SelectedKeyName, BestDist);
                }
                if (!AngleKey.IsNone())
                {
                    float AbsAngle = FMath::Abs(Self->GetAngleToTarget(BestTarget));
                    BB->SetValueAsFloat(AngleKey.SelectedKeyName, AbsAngle);
                }

                if (ANonCharacterBase* PlayerChar = Cast<ANonCharacterBase>(BestTarget))
                {
                    PlayerChar->EnterCombatState();
                }
            }
        }
    }

    // ── 3) 어그로/복귀 유무에 따른 보스/몬스터 개별 이동 속도(Walk vs Run) 자동 조절 ─────────
    if (UCharacterMovementComponent* MoveComp = Self->GetCharacterMovement())
    {
        // 몬스터 개별 캐릭터 블루프린트에 지정된 속도가 있으면 최우선 반영!
        float ActualRunSpeed = (Self->CombatRunSpeed > 0.0f) ? Self->CombatRunSpeed : CombatRunSpeed;
        float ActualWalkSpeed = (Self->PatrolWalkSpeed > 0.0f) ? Self->PatrolWalkSpeed : PatrolWalkSpeed;

        AActor* FinalTarget = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
        if (FinalTarget)
        {
            // 어그로 잡힘 ➔ 해당 몬스터 고유 달리기 속도 적용
            MoveComp->MaxWalkSpeed = ActualRunSpeed;
        }
        else
        {
            // 타겟 없음: 이전에 어그로가 끌렸다가 스폰 지역으로 리셋 복귀 중이면 달리기(Run), 스폰 구역 근처나 평상시 순찰 시에는 걷기(Walk)
            float DistFromHome = FVector::Dist2D(Self->GetActorLocation(), Self->SpawnLocation);
            if (Self->IsAggro() && DistFromHome > 300.0f)
            {
                MoveComp->MaxWalkSpeed = ActualRunSpeed; // 리셋 빠른 복귀
            }
            else
            {
                MoveComp->MaxWalkSpeed = ActualWalkSpeed; // 평상시 순찰 걷기 (Patrol)
            }
        }
    }
}

