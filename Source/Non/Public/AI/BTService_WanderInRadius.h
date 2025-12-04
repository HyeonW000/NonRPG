#pragma once
#include "BehaviorTree/BTService.h"
#include "BTService_WanderInRadius.generated.h"

UENUM(BlueprintType)
enum class EWanderCenterSource : uint8
{
    PawnSpawn     UMETA(DisplayName = "Pawn Spawn Location"),
    BlackboardKey UMETA(DisplayName = "Blackboard Vector Key")
};

UCLASS(meta = (DisplayName = "Wander In Radius (C++)"))
class NON_API UBTService_WanderInRadius : public UBTService
{
    GENERATED_BODY()
public:
    UBTService_WanderInRadius();

    // 업데이트할 위치 키(패트롤 목적지)
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector PatrolLocationKey; // Vector

    // 중심점 소스
    UPROPERTY(EditAnywhere, Category = "Center")
    EWanderCenterSource CenterSource = EWanderCenterSource::PawnSpawn;

    // CenterSource=BlackboardKey일 때 사용
    UPROPERTY(EditAnywhere, Category = "Center", meta = (EditCondition = "CenterSource==EWanderCenterSource::BlackboardKey"))
    FBlackboardKeySelector CenterVectorKey;

    // 배회 반경
    UPROPERTY(EditAnywhere, Category = "Wander")
    float WanderRadius = 800.f;

    // 목표 유지 조건: 현재 목표가 Pawn에서 이 거리 이상이면 유지 (MoveTo AR의 ~2배 권장)
    UPROPERTY(EditAnywhere, Category = "Wander")
    float KeepIfFartherThan = 400.f;

    // 목표를 너무 자주 바꾸지 않기 위한 최소 간격
    UPROPERTY(EditAnywhere, Category = "Wander")
    float MinUpdateInterval = 0.8f;

    // 새 목표 랜덤성(작게 주면 덜 흔듦)
    UPROPERTY(EditAnywhere, Category = "Wander")
    float JitterRadius = 150.f;

    // 디버그 표시
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDebugDraw = false;

    UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition = "bDebugDraw"))
    float DebugDrawTime = 1.5f;

protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
    // 컨트롤러별 마지막 갱신시간 캐시
    TMap<TWeakObjectPtr<class AAIController>, float> LastSetTimeMap;

    FVector ResolveCenter(class AAIController* AIC, class UBlackboardComponent* BB) const;
    bool    PickReachable(UWorld* World, const FVector& Center, float Radius, FVector& Out) const;
};
