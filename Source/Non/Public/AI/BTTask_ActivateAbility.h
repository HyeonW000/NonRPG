#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ActivateAbility.generated.h"

class UGameplayAbility;

/**
 * 지정된 GAS 어빌리티(AbilityClass)를 발동시키고, 
 * 어빌리티(회전 몽타주 등)가 끝날 때까지 비헤이비어 트리를 기다리게 해주는 Task
 */
UCLASS(meta = (DisplayName = "Activate Ability (C++)"))
class NON_API UBTTask_ActivateAbility : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_ActivateAbility();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

    // 발동시킬 어빌리티 클래스 (예: GA_BossTurn_BP)
    UPROPERTY(EditAnywhere, Category = "GAS")
    TSubclassOf<UGameplayAbility> AbilityClass;

private:
    float LogTimer = 0.0f;
    float CleanupWaitTimer = 0.0f;
    bool bIsCleaningUp = false;
};
