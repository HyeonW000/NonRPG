#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NonAIController.generated.h"

UCLASS()
class NON_API ANonAIController : public AAIController
{
    GENERATED_BODY()
public:
    ANonAIController();

    virtual FPathFollowingRequestResult MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath = nullptr) override;

    // 일반 몬스터용 비헤이비어 트리 자산 (기본값)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI")
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset = nullptr;

    // 보스 몬스터 전용 비헤이비어 트리 자산 (ABossCharacter 또는 bIsBoss 빙의 시 자동 우선 실행)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI")
    TObjectPtr<UBehaviorTree> BossBehaviorTreeAsset = nullptr;

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

private:
    UPROPERTY()
    UBlackboardComponent* BBComp = nullptr;
};
