#pragma once
#include "BehaviorTree/BTService.h"
#include "BTService_UpdateTarget.generated.h"

UCLASS(meta = (DisplayName = "Update Target (C++)"))
class NON_API UBTService_UpdateTarget : public UBTService
{
    GENERATED_BODY()
public:
    UBTService_UpdateTarget();

    /** 타겟을 기록할 BB 키 (Object/Actor 타입) */
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetActorKey;

    /** 타겟과의 거리를 기록할 BB 키 (Float 타입) */
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector DistanceKey;

    /** 타겟과의 상대 각도 크기를 기록할 BB 키 (Float 타입) */
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector AngleKey;

    /** 반경 히스테리시스 */
    UPROPERTY(EditAnywhere, Category = "Sense")
    float EnterRadius = 1000.f;   // 안으로 들어오면 타겟 획득
    UPROPERTY(EditAnywhere, Category = "Sense")
    float ExitRadius = 1400.f;   // 밖으로 나가면 타겟 해제

    /** 토글 방지 최소 유지시간(진입/이탈 별도) */
    UPROPERTY(EditAnywhere, Category = "Detect")
    float MinHoldTimeOnEnter = 0.0f;   // 처음 잡을 때 지연 없음(권장 0~0.2)
    UPROPERTY(EditAnywhere, Category = "Detect")
    float MinHoldTimeOnExit = 0.5f;   // 잃을 땐 튐 방지(권장 0.3~0.8)

    /** EnemyCharacter의 AggroStyle(Preemptive/Reactive)을 존중할지 */
    UPROPERTY(EditAnywhere, Category = "Sense")
    bool bRespectAggroStyle = true;

    // 스폰 지점 기준 리쉬
    UPROPERTY(EditAnywhere, Category = "Sense")
    bool bUseHomeLeash = true;

    UPROPERTY(EditAnywhere, Category = "Sense", meta = (EditCondition = "bUseHomeLeash"))
    float HomeLeashRadius = 2500.f;

    /** 복귀 락 해제 반경: 스폰 위치에서 이 반경 안으로 들어오면 1차 스캔을 통해 플레이어 재감지 허용 (1200~1500cm 권장) */
    UPROPERTY(EditAnywhere, Category = "Sense", meta = (EditCondition = "bUseHomeLeash"))
    float HomeLeashReleaseRadius = 1300.f;

    /** 전투 무반응 타임아웃: 서로 아무 피해도 주거나 입지 않은 상태로 이 시간(초)이 지나면 스폰 위치로 복귀 (기본 8초) */
    UPROPERTY(EditAnywhere, Category = "Sense", meta = (ClampMin = "1.0"))
    float CombatInactivityTimeout = 8.0f;

    /** 평상시/순찰 걷기 속도 (DT_EnemyData에서 자동 관리) */
    UPROPERTY()
    float PatrolWalkSpeed = 200.f;

    /** 어그로/전투 및 리셋 복귀 달리기 속도 (DT_EnemyData에서 자동 관리) */
    UPROPERTY()
    float CombatRunSpeed = 400.f;

protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
    /** 마지막 타겟 상태 전환 시각 */
    float LastSwitchTime = -1.f;
};
