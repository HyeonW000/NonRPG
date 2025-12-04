#include "AI/BTService_UpdateTarget.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "AI/EnemyCharacter.h"
#include "GameFramework/Pawn.h"

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
    const float Now = AIC->GetWorld()->GetTimeSeconds();
    AActor* Curr = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
    const float Dist = FVector::Dist2D(Self->GetActorLocation(), Player->GetActorLocation());

    // AggroStyle 반영 여부
    const bool bReactiveMode =
        bRespectAggroStyle ? (Self->AggroStyle == EAggroStyle::Reactive) : false;

    // ── 현재 타겟 유지/해제 ─────────────────────────────────────────
    if (Curr)
    {
        // 해제 조건: ExitRadius 벗어남 + 최소 유지시간(Exit)
        if (Dist > ExitRadius && (Now - LastSwitchTime) >= MinHoldTimeOnExit)
        {
            BB->ClearValue(TargetActorKey.SelectedKeyName);
            LastSwitchTime = Now;
        }
        return; // 타겟 유지 중이면 더 이상 처리 안함 (_restart 방지)
    }

    // ── 신규 타겟 획득 ────────────────────────────────────────────
    if (Dist < EnterRadius && (Now - LastSwitchTime) >= MinHoldTimeOnEnter)
    {
        bool bCanAggro = true;

        if (bReactiveMode)
        {
            // 맞아서 어그로가 들어왔고, 그 상태가 아직 유지되는지(홀드타임 내) 확인
            const bool bAggroFlag = Self->IsAggroByHit();
            const bool bWithinHold = (Now - Self->GetLastAggroByHitTime()) <= Self->GetAggroByHitHoldTime();
            bCanAggro = (bAggroFlag && bWithinHold);
        }

        if (bCanAggro)
        {
            BB->SetValueAsObject(TargetActorKey.SelectedKeyName, Player);
            LastSwitchTime = Now;

            // 사정거리 진입 타임스탬프 (첫 공격 텀용)
            Self->MarkEnteredAttackRange();
        }
    }
}
