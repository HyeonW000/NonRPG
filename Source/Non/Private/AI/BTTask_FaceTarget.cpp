#include "AI/BTTask_FaceTarget.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

UBTTask_FaceTarget::UBTTask_FaceTarget()
{
    NodeName = TEXT("Face Target (Wait for Animation)");
    bNotifyTick = true;
    
    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FaceTarget, TargetKey), AActor::StaticClass());
    TargetKey.SelectedKeyName = FName("TargetActor"); 

    AcceptableAngle = 25.0f; // 15도에서 25도로 완화 (깜빡임 방지)
}

EBTNodeResult::Type UBTTask_FaceTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;
    if (!AIC || !Pawn) return EBTNodeResult::Failed;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    // [New] 공격 중일 때는 회전 로직을 완전히 차단합니다.
    if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
    {
        if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"))))
        {
            return EBTNodeResult::Succeeded;
        }
    }

    AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
    if (!Target) return EBTNodeResult::Failed;

    CachedTarget = Target;

    // 1. AI 컨트롤러의 ControlRotation을 타겟 방향으로 부드럽게 세팅합니다 (ClearFocus 틱 튕김 제거)
    FVector LookDir = (Target->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D();
    const FRotator TargetRot = LookDir.Rotation();
    AIC->SetControlRotation(TargetRot);

    // 2. 현재 각도 체크
    FVector ForwardDir = Pawn->GetActorForwardVector().GetSafeNormal2D();
    float AngleDiff = FMath::Abs(FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ForwardDir, LookDir))));

    if (AngleDiff <= AcceptableAngle)
    {
        return EBTNodeResult::Succeeded;
    }

    // 아직 정면이 아니면 애니메이션이 돌 때까지 기다립니다. (TickTask로 진입)
    return EBTNodeResult::InProgress;
}

void UBTTask_FaceTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;

    if (!AIC || !Pawn || !CachedTarget.IsValid())
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    AActor* Target = CachedTarget.Get();

    // 목표 방향 시선 실시간 동기화
    FVector LookDir = (Target->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D();
    const FRotator TargetRot = LookDir.Rotation();
    AIC->SetControlRotation(TargetRot);

    // 목표 방향과 현재 정면 방향 사이의 각도 차이를 계산합니다.
    FVector ForwardDir = Pawn->GetActorForwardVector().GetSafeNormal2D();
    
    // DotProduct를 사용하여 0~180도 사이의 절대적인 차이를 구합니다.
    float AngleDiff = FMath::Abs(FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ForwardDir, LookDir))));

    // 애니메이션(루트 모션)에 의해 보스의 몸이 돌아가서 각도가 맞으면 성공!
    if (AngleDiff <= AcceptableAngle)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}
