#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NonAIController.generated.h"

UCLASS()
class NON_API ANonAIController : public AAIController
{
    GENERATED_BODY()
public:
    ANonAIController();

    // 에디터에서 BP_NonAIController에 꽂을 BT 자산
    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBehaviorTree* BehaviorTreeAsset = nullptr;

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

private:
    UPROPERTY()
    UBlackboardComponent* BBComp = nullptr;
};
