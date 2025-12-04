#include "AI/BTService_WanderInRadius.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"
#include "AI/EnemyCharacter.h" // SpawnLocation 접근용 (네 경로에 맞게)

UBTService_WanderInRadius::UBTService_WanderInRadius()
{
    bNotifyTick = true;
    Interval = 0.6f;       // 너무 잦지 않게
    RandomDeviation = 0.f;
    NodeName = TEXT("Wander In Radius (C++)");
}

FVector UBTService_WanderInRadius::ResolveCenter(AAIController* AIC, UBlackboardComponent* BB) const
{
    APawn* P = AIC ? AIC->GetPawn() : nullptr;
    if (!P) return FVector::ZeroVector;

    if (CenterSource == EWanderCenterSource::BlackboardKey && BB)
    {
        const FVector FromBB = BB->GetValueAsVector(CenterVectorKey.SelectedKeyName);
        if (!FromBB.IsNearlyZero()) return FromBB;
    }
    // 기본: Pawn SpawnLocation (없으면 현재 위치)
    if (const AEnemyCharacter* EC = Cast<AEnemyCharacter>(P))
    {
        return EC->SpawnLocation.IsNearlyZero() ? P->GetActorLocation() : EC->SpawnLocation;
    }
    return P->GetActorLocation();
}

bool UBTService_WanderInRadius::PickReachable(UWorld* World, const FVector& Center, float Radius, FVector& Out) const
{
    if (!World) return false;
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
    if (!NavSys) return false;

    // 반경 안에서 랜덤 + 지터 섞기
    for (int32 Try = 0; Try < 8; ++Try)
    {
        const FVector RandDir = FMath::VRand().GetSafeNormal2D();
        const float   RandLen = FMath::FRandRange(FMath::Max(0.f, Radius - JitterRadius), Radius);
        const FVector Cand = Center + RandDir * RandLen;

        FNavLocation Loc;
        if (NavSys->ProjectPointToNavigation(Cand, Loc, FVector(Radius, Radius, 200.f)))
        {
            Out = Loc.Location;
            return true;
        }
    }
    return false;
}

void UBTService_WanderInRadius::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    AAIController* AIC = OwnerComp.GetAIOwner();
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!AIC || !BB) return;

    UWorld* World = AIC->GetWorld();
    APawn* Pawn = AIC->GetPawn();
    if (!World || !Pawn) return;

    const float Now = World->GetTimeSeconds();
    const float* pLast = LastSetTimeMap.Find(AIC);
    const float Last = pLast ? *pLast : -1000.f;

    // 현재 목표 유지 조건 점검
    const FVector Curr = BB->GetValueAsVector(PatrolLocationKey.SelectedKeyName);
    if (!Curr.IsNearlyZero())
    {
        const float Dist = FVector::Dist2D(Pawn->GetActorLocation(), Curr);
        if (Dist >= KeepIfFartherThan && (Now - Last) < MinUpdateInterval)
        {
            // 충분히 멀고, 최솟간격 내면 유지
            if (bDebugDraw)
            {
                DrawDebugSphere(World, Curr, 28.f, 12, FColor::Yellow, false, DebugDrawTime, 0, 2.f);
                DrawDebugLine(World, Pawn->GetActorLocation(), Curr, FColor::Yellow, false, DebugDrawTime, 0, 1.5f);
                DrawDebugString(World, Curr + FVector(0, 0, 40), TEXT("KEEP"), nullptr, FColor::Yellow, DebugDrawTime);
            }
            return;
        }
    }

    // 새 목표 선택
    const FVector Center = ResolveCenter(AIC, BB);
    FVector Picked;
    if (PickReachable(World, Center, WanderRadius, Picked))
    {
        // 경로 유효성 간단 체크 (선택 사항)
        if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World))
        {
            if (UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(World, Pawn->GetActorLocation(), Picked, Pawn))
            {
                if (!(Path && Path->IsValid() && Path->PathPoints.Num() >= 2))
                {
                    // 경로 이상하면 다음 틱에 다시 시도
                    return;
                }
            }
        }

        BB->SetValueAsVector(PatrolLocationKey.SelectedKeyName, Picked);
        LastSetTimeMap.Add(AIC, Now);

        if (bDebugDraw)
        {
            DrawDebugSphere(World, Picked, 30.f, 16, FColor::Green, false, DebugDrawTime, 0, 2.f);
            DrawDebugLine(World, Pawn->GetActorLocation(), Picked, FColor::Green, false, DebugDrawTime, 0, 1.5f);
            DrawDebugCircle(World, Center, WanderRadius, 32, FColor::Cyan, false, DebugDrawTime, 0, 1.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
        }
    }
}
