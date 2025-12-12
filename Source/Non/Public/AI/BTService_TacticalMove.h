#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_TacticalMove.generated.h"

/**
 * 전술적 이동 (비전투 걷기/대기가 아닌, 전투 중 위치 선정)
 * - Strafe: 타겟을 중심으로 회전 이동
 * - KeepDistance: 타겟과 일정 거리 유지 (Backstep)
 */
UCLASS(meta = (DisplayName = "Tactical Move (C++)"))
class NON_API UBTService_TacticalMove : public UBTService
{
    GENERATED_BODY()
    
public:
    UBTService_TacticalMove();

    /** 이동 목표 지점 (Vector) */
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetLocationKey;

    /** 기준 대상 (Actor - Player) */
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetActorKey;

    /** 이 거리보다 가까우면 뒤로 물러남 (Backstep) */
    UPROPERTY(EditAnywhere, Category = "Tactics")
    float ToCloseDistance = 250.f;

    /** 이 거리보다 멀면 전술 이동 안함 (그냥 추적 로직에 맡김) */
    UPROPERTY(EditAnywhere, Category = "Tactics")
    float MaxTacticalRange = 600.f;

    /** Strafe(좌우) 이동 확률 (0~1) */
    UPROPERTY(EditAnywhere, Category = "Tactics")
    float StrafeChance = 0.7f;

    /** Strafe 시 이동할 반경 (작게 끊어 움직이기) */
    UPROPERTY(EditAnywhere, Category = "Tactics")
    float StrafeStepDistance = 300.f;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDebugDraw = false;

protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
    bool GetStrafeLocation(UWorld* World, const FVector& Origin, const FVector& Target, FVector& OutLoc) const;
    bool GetBackstepLocation(UWorld* World, const FVector& Origin, const FVector& Target, FVector& OutLoc) const;
};
