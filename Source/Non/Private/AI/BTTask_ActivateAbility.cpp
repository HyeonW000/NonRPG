#include "AI/BTTask_ActivateAbility.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbility.h"
#include "Character/EnemyCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/Engine.h"

UBTTask_ActivateAbility::UBTTask_ActivateAbility()
{
    NodeName = TEXT("Activate Ability (C++)");
    bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_ActivateAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    LogTimer = 0.0f;
    CleanupWaitTimer = 0.0f;
    bIsCleaningUp = false;

    if (!AbilityClass)
    {
        return EBTNodeResult::Failed;
    }

    AAIController* AIC = OwnerComp.GetAIOwner();
    if (!AIC) return EBTNodeResult::Failed;

    APawn* Pawn = AIC->GetPawn();
    if (!Pawn) return EBTNodeResult::Failed;

    IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Pawn);
    if (!ASI) return EBTNodeResult::Failed;

    UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
    if (!ASC) return EBTNodeResult::Failed;

    // 보스가 보유한 어빌리티 중 AbilityClass의 자식 클래스(예: GA_BossTurn_Golem)를 포함하여 매칭 후 발동
    bool bSuccess = false;
    for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
    {
        if (Spec.Ability && Spec.Ability->GetClass()->IsChildOf(AbilityClass))
        {
            bSuccess = ASC->TryActivateAbility(Spec.Handle);
            if (bSuccess)
            {
                // 회전 몽타주 재생 중 AI 시선 강제 꺾임 경쟁(떨림)을 차단하기 위해 Focus 해제
                AIC->ClearFocus(EAIFocusPriority::Gameplay);
                break;
            }
        }
    }

    if (!bSuccess)
    {
        return EBTNodeResult::Failed;
    }

    return EBTNodeResult::InProgress;
}

void UBTTask_ActivateAbility::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;
    IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Pawn);
    UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;

    if (!ASC)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    // 0.3초마다 턴 진행 수치 및 잔여 각도 Output Log 출력
    LogTimer += DeltaSeconds;
    if (LogTimer >= 0.3f)
    {
        LogTimer = 0.0f;
        if (AEnemyCharacter* Boss = Cast<AEnemyCharacter>(Pawn))
        {
            AActor* TargetActor = nullptr;
            if (UBlackboardComponent* BB = AIC ? AIC->GetBlackboardComponent() : nullptr)
            {
                TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
            }
            float AngleToTarget = TargetActor ? Boss->GetAngleToTarget(TargetActor) : 0.0f;
            UE_LOG(LogTemp, Warning, TEXT("🔄 [회전 진행 중] 보스 현재 Yaw: %.1f° | 남아있는 각도: %.1f°"), Boss->GetActorRotation().Yaw, AngleToTarget);
        }
    }

    // 몽타주가 끝난 직후 어빌리티 엔진 슬롯 정리를 위한 0.1초 대기 유예
    if (bIsCleaningUp)
    {
        CleanupWaitTimer += DeltaSeconds;
        if (CleanupWaitTimer >= 0.1f)
        {
            bIsCleaningUp = false;
            FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        }
        return;
    }

    if (!ASC->GetAnimatingAbility() && !ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Turn"), false)))
    {
        if (AEnemyCharacter* Boss = Cast<AEnemyCharacter>(Pawn))
        {
            AActor* TargetActor = nullptr;
            if (UBlackboardComponent* BB = AIC ? AIC->GetBlackboardComponent() : nullptr)
            {
                TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
            }
            float AngleToTarget = TargetActor ? Boss->GetAngleToTarget(TargetActor) : 0.0f;
            UE_LOG(LogTemp, Warning, TEXT("✅ [회전 완료!] 최종 보스 Yaw: %.1f° | 최종 남아있는 잔여 각도: %.1f°"), Boss->GetActorRotation().Yaw, AngleToTarget);
        }

        // 바로 트리를 끝내지 않고 0.1초 대기 유예를 시작하여 어빌리티 완전 해제(End) 보장
        bIsCleaningUp = true;
        CleanupWaitTimer = 0.0f;
    }
}

void UBTTask_ActivateAbility::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
    Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);

    // 어빌리티 종료 시 AI Focus 복구
    if (AAIController* AIC = OwnerComp.GetAIOwner())
    {
        if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
        {
            if (AActor* Target = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor"))))
            {
                AIC->SetFocus(Target);
            }
        }
    }
}
