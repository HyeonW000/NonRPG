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

    // 1. AI 컨트롤러에게 타겟을 집중(Focus)하도록 명령합니다.
    // 이는 캐릭터의 '몸'을 돌리는 것이 아니라 '컨트롤러 각도'만 플레이어 쪽으로 고정시킵니다.
    // 이 덕분에 AnimInstance의 RootYawOffset(ControlRotation - ActorRotation)이 정확한 값을 갖게 됩니다.
    AIC->SetFocus(Target);

    // 2. 현재 각도 체크
    FVector LookDir = (Target->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D();
    FVector ForwardDir = Pawn->GetActorForwardVector().GetSafeNormal2D();
    float AngleDiff = FMath::Abs(FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ForwardDir, LookDir))));

    if (AngleDiff <= AcceptableAngle)
    {
        AIC->ClearFocus(EAIFocusPriority::Gameplay);
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

    // 목표 방향과 현재 정면 방향 사이의 각도 차이를 계산합니다.
    FVector LookDir = (Target->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D();
    FVector ForwardDir = Pawn->GetActorForwardVector().GetSafeNormal2D();
    
    // DotProduct를 사용하여 0~180도 사이의 절대적인 차이를 구합니다.
    float AngleDiff = FMath::Abs(FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ForwardDir, LookDir))));

    // 디버깅 메시지
    if (GEngine)
    {
        FString Msg = FString::Printf(TEXT("[FaceTarget] Waiting for Animation... Diff: %.2f"), AngleDiff);
        GEngine->AddOnScreenDebugMessage(1, DeltaSeconds, FColor::Orange, Msg);
    }

    // 애니메이션(루트 모션)에 의해 보스의 몸이 돌아가서 각도가 맞으면 성공!
    if (AngleDiff <= AcceptableAngle)
    {
        AIC->ClearFocus(EAIFocusPriority::Gameplay);
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}
