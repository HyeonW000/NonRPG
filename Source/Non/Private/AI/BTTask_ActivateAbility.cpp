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

    bool bSuccess = false;
    for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
    {
        if (Spec.Ability && AbilityClass && (Spec.Ability->GetClass()->IsChildOf(AbilityClass) || AbilityClass->IsChildOf(Spec.Ability->GetClass())))
        {
            bSuccess = ASC->TryActivateAbility(Spec.Handle);
            if (bSuccess)
            {
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

    if (ASC)
    {
        bool bHasTurnTag = ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Turn"), false));
        bool bHasAttackingTag = ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false));

        if (!bHasTurnTag && !bHasAttackingTag)
        {
            FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        }
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
