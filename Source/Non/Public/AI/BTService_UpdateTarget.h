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

protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
    /** 마지막 타겟 상태 전환 시각 */
    float LastSwitchTime = -1.f;
};
