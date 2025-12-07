#include "AI/BTService_UpdateTarget.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "AI/EnemyCharacter.h"
#include "GameFramework/Pawn.h"
#include "Character/NonCharacterBase.h"

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

    const bool bReactiveMode =
        bRespectAggroStyle ? (Self->AggroStyle == EAggroStyle::Reactive) : false;

    // ── 1) 타겟 유지/해제 검사 ───────────────────────
    if (Curr)
    {
        bool bLoseTarget = false;

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
        return;
    }

    // ── 2) 신규 타겟 획득 ───────────────────────────
    if (DistToPlayer < EnterRadius && (Now - LastSwitchTime) >= MinHoldTimeOnEnter)
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
            BB->SetValueAsObject(TargetActorKey.SelectedKeyName, Player);
            LastSwitchTime = Now;

            // 어그로 시작 플래그
            Self->SetAggro(true);

            // 사정거리 진입 시각 기록 (웜업용)
            Self->MarkEnteredAttackRange();

            // 어그로 시작 자체도 전투로 간주하고 싶다면:
            if (ANonCharacterBase* PlayerChar = Cast<ANonCharacterBase>(Player))
            {
                PlayerChar->EnterCombatState();
            }
        }
    }
}

