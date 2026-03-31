#include "AI/BTTask_FaceTarget.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

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

    // 2. 기절, 피격, 사망 상태인지 확인하여 회전 차단
    if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
    {
        if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"))) ||
            ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.HitReacting"))) ||
            ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead"))))
        {
            // 상태 이상 중에는 회전하지 않고 성공 처리 (BT 흐름 방해 안 함)
            return EBTNodeResult::Succeeded;
        }
    }

    // 3. 즉시 회전 (SetActorRotation) 
    // 공격 직전 "바라보기"는 즉시 맞추거나, 아주 빠르게 보간해야 함.
    // 여기서는 간단하게 즉시 회전 + Controller 회전 동기화
    Pawn->SetActorRotation(TargetRot);
    AIC->SetControlRotation(TargetRot);

    return EBTNodeResult::Succeeded;
}
