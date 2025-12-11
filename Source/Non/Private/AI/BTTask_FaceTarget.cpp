#include "AI/BTTask_FaceTarget.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/KismetMathLibrary.h"

UBTTask_FaceTarget::UBTTask_FaceTarget()
{
    NodeName = TEXT("Face Target");
    
    // 기본 키 이름 (필요 시 에디터 변경 가능)
    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FaceTarget, TargetKey), AActor::StaticClass());
    TargetKey.SelectedKeyName = FName("TargetActor"); 
}

EBTNodeResult::Type UBTTask_FaceTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;

    if (!AIC || !Pawn)
    {
        return EBTNodeResult::Failed;
    }

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
    if (!Target)
    {
        return EBTNodeResult::Failed;
    }

    // 1. 방향 계산
    FVector Dir = Target->GetActorLocation() - Pawn->GetActorLocation();
    Dir.Z = 0.f; // 높이는 무시

    if (Dir.IsNearlyZero())
    {
        return EBTNodeResult::Succeeded; // 이미 겹쳐있음
    }

    const FRotator TargetRot = Dir.Rotation();

    // 2. 즉시 회전 (SetActorRotation) or 부드러운 회전? 
    // 공격 직전 "바라보기"는 즉시 맞추거나, 아주 빠르게 보간해야 함.
    // 여기서는 간단하게 즉시 회전 + Controller 회전 동기화
    Pawn->SetActorRotation(TargetRot);
    AIC->SetControlRotation(TargetRot);

    return EBTNodeResult::Succeeded;
}
