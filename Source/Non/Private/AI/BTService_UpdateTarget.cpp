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
    APawn* Player = UGameplayStatics::GetPlayerPawn(AIC, 0);
    if (!Self || !Player) return;

    if (Self->IsSpawnFading())
    {
        return;
    }

    UWorld* World = AIC->GetWorld();
    const float Now = World ? World->GetTimeSeconds() : 0.f;

    AActor* Curr = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
    const float DistToPlayer = FVector::Dist2D(Self->GetActorLocation(), Player->GetActorLocation());

    // [New] 블랙보드에 실시간 거리 기록
    if (!DistanceKey.IsNone())
    {
        BB->SetValueAsFloat(DistanceKey.SelectedKeyName, DistToPlayer);
    }

    // [New] 블랙보드에 실시간 상대 각도(절댓값) 기록
    if (!AngleKey.IsNone())
    {
        float AbsAngle = FMath::Abs(Self->GetAngleToTarget(Player));
        BB->SetValueAsFloat(AngleKey.SelectedKeyName, AbsAngle);
    }

    const bool bReactiveMode =
        bRespectAggroStyle ? (Self->AggroStyle == EAggroStyle::Reactive) : false;

    // ── 1) 타겟 유지/해제 검사 ───────────────────────
    if (Curr)
    {
        bool bLoseTarget = false;

        // 0) 플레이어가 죽었는지 검사
        if (ANonCharacterBase* TargetChar = Cast<ANonCharacterBase>(Curr)) {
            if (TargetChar->IsDead()) {
                bLoseTarget = true;
            }
        }

        // 1) 플레이어가 ExitRadius 밖으로 나감 + 최소 유지시간
        if (DistToPlayer > ExitRadius && (Now - LastSwitchTime) >= MinHoldTimeOnExit)
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
        }
        else
        {
            // 타겟을 정상 유지하고 있으면 달리기 속도 유지 후 종료
            if (UCharacterMovementComponent* MoveComp = Self->GetCharacterMovement())
            {
                MoveComp->MaxWalkSpeed = CombatRunSpeed;
            }
            return;
        }
    }

    // ── 2) 신규 타겟 획득 ───────────────────────────
    if (DistToPlayer < EnterRadius && (Now - LastSwitchTime) >= MinHoldTimeOnEnter)
    {
        // [Fix] 플레이어가 죽었는지 먼저 확인하고 죽었으면 신규 타겟으로 잡지 않습니다!
        if (ANonCharacterBase* TargetChar = Cast<ANonCharacterBase>(Player)) {
            if (TargetChar->IsDead()) {
                return;
            }
        }

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
            BB->SetValueAsObject(TargetActorKey.SelectedKeyName, Player);
            LastSwitchTime = Now;

            // 어그로 시작 플래그
            Self->SetAggro(true);

            // 사정거리 진입 시각 기록 (웜업용)
            Self->MarkEnteredAttackRange();
            


            if (ANonCharacterBase* PlayerChar = Cast<ANonCharacterBase>(Player))
            {
                PlayerChar->EnterCombatState();
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

