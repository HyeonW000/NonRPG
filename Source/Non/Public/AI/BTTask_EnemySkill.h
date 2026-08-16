#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"
#include "BTTask_EnemySkill.generated.h"

class UGameplayAbility;

UCLASS(meta = (DisplayName = "Perform Enemy Skill (C++)"))
class NON_API UBTTask_EnemySkill : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_EnemySkill();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

    // 특정 태그를 가진 스킬만 골라서 사용하고 싶을 때 설정 (비어있으면 모든 스킬 대상)
    UPROPERTY(EditAnywhere, Category = "EnemyAI")
    FGameplayTag CategoryTag;

    // 여러 스킬 중 무작위로 고를지, 아니면 순번대로 고를지
    UPROPERTY(EditAnywhere, Category = "EnemyAI")
    bool bRandomSkillSelection = true;

    // 적용될 최소 거리 (이보다 가까우면 실패)
    UPROPERTY(EditAnywhere, Category = "EnemyAI")
    float MinDistance = 0.f;

    // 적용될 최대 거리 (이보다 멀면 실패)
    UPROPERTY(EditAnywhere, Category = "EnemyAI")
    float MaxDistance = 99999.f;

    // 스킬을 발동할 수 있는 타겟과의 최대 허용 정면 각도 (예: 45도. 이 각도보다 정면을 안 바라보고 있으면 스킬 발동 실패 -> Turn 유도!)
    UPROPERTY(EditAnywhere, Category = "EnemyAI")
    float MaxFacingAngle = 180.0f;

    // 실행 확률 (0.0 ~ 1.0)
    UPROPERTY(EditAnywhere, Category = "EnemyAI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ActivateProbability = 1.0f;

    // 거리 계산에 사용할 타격 대상 블랙보드 키
    UPROPERTY(EditAnywhere, Category = "EnemyAI")
    FBlackboardKeySelector TargetActorKey;

private:
    FDelegateHandle AbilityEndedHandle;
    void OnAbilityEndedCallback(UGameplayAbility* EndedAbility, UBehaviorTreeComponent* OwnerComp);
};
