#include "AI/BTService_TacticalMove.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "DrawDebugHelpers.h"

UBTService_TacticalMove::UBTService_TacticalMove()
{
    bNotifyTick = true;
    Interval = 1.5f;        // 0.5f -> 1.5f (너무 정신없지 않게 수정)
    RandomDeviation = 0.2f;
    NodeName = TEXT("Tactical Move (C++)");
}

void UBTService_TacticalMove::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    AAIController* AIC = OwnerComp.GetAIOwner();
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!AIC || !BB) return;

    APawn* Pawn = AIC->GetPawn();
    if (!Pawn) return;

    // 타겟(플레이어) 확인
    AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
    if (!TargetActor) return;

    const FVector MyLoc = Pawn->GetActorLocation();
    const FVector TargetLoc = TargetActor->GetActorLocation();
    const float Dist = FVector::Dist2D(MyLoc, TargetLoc);

    FVector NewDest = FVector::ZeroVector;
    bool bFound = false;
    UWorld* World = AIC->GetWorld();

    // 1. 너무 가까우면 -> Backstep (회피)
    if (Dist < ToCloseDistance)
    {
        bFound = GetBackstepLocation(World, MyLoc, TargetLoc, NewDest);
    }
    // 2. 적정 사거리 안쪽이면 -> Strafe (좌우) 시도
    else if (Dist <= MaxTacticalRange)
    {
        // Strafe 확률 체크
        if (FMath::FRand() <= StrafeChance)
        {
             bFound = GetStrafeLocation(World, MyLoc, TargetLoc, NewDest);
        }
        else
        {
            // 확률 안 걸리면 그냥 타겟 바라보거나 제자리 대기
            // 여기서는 '현재 위치' 혹은 '타겟 위치'를 넣어서 
            // 너무 멀어지지 않게 관리할 수도 있음.
            // 일단은 bFound = false 로 둬서 이전 이동 유지 or 멈춤
        }
    }
    // 3. 멀면 -> 추격(Chase)
    //    Behavior Tree 구조상 "추격" 노드가 따로 없다면, 여기서 위치를 갱신해줘야 함.
    else
    {
        NewDest = TargetLoc;
        bFound = true;
    }

    if (bFound)
    {
        BB->SetValueAsVector(TargetLocationKey.SelectedKeyName, NewDest);
        
        if (bDebugDraw && World)
        {
            DrawDebugSphere(World, NewDest, 30.f, 12, FColor::Purple, false, 0.6f);
            DrawDebugLine(World, MyLoc, NewDest, FColor::Purple, false, 0.6f, 0, 2.f);
        }
    }
}

bool UBTService_TacticalMove::GetStrafeLocation(UWorld* World, const FVector& Origin, const FVector& Target, FVector& OutLoc) const
{
    if (!World) return false;
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
    if (!NavSys) return false;

    // Target -> Me 벡터
    FVector Dir = (Origin - Target).GetSafeNormal2D();
    
    // 왼쪽(-90) 혹은 오른쪽(+90) 중 랜덤 선택. 
    // 약간의 전진/후진도 섞어서 완전히 원이 아니게 (45~135도 사이)
    const bool bRight = FMath::RandBool();
    const float BaseAngle = bRight ? 90.f : -90.f;
    const float RandomAngle = FMath::FRandRange(-20.f, 20.f); 

    const FVector RotateDir = Dir.RotateAngleAxis(BaseAngle + RandomAngle, FVector::UpVector);

    // 목표점 후보
    const FVector Candidate = Origin + RotateDir * StrafeStepDistance;

    // NavMesh 위에 있는지 확인
    FNavLocation NavLoc;
    // ProjectPointToNavigation: 후보 지점 근처 NavMesh 찾기
    if (NavSys->ProjectPointToNavigation(Candidate, NavLoc, FVector(100, 100, 100)))
    {
        // 벽 너머인지 체크 (Raycast) - 간단하게만
        FHitResult Hit;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(Cast<AActor>(World->GetFirstPlayerController()->GetPawn())); // 임시
        
        // 경로 유효성까지 체크하면 좋음
        OutLoc = NavLoc.Location;
        return true;
    }

    return false;
}

bool UBTService_TacticalMove::GetBackstepLocation(UWorld* World, const FVector& Origin, const FVector& Target, FVector& OutLoc) const
{
    if (!World) return false;
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
    if (!NavSys) return false;

    // Target에서 멀어지는 방향
    FVector Dir = (Origin - Target).GetSafeNormal2D();
    
    // 뒤로 조금 물러남
    const FVector Candidate = Origin + Dir * (StrafeStepDistance * 0.8f); 

    FNavLocation NavLoc;
    if (NavSys->ProjectPointToNavigation(Candidate, NavLoc, FVector(100, 100, 100)))
    {
        OutLoc = NavLoc.Location;
        return true;
    }
    return false;
}
