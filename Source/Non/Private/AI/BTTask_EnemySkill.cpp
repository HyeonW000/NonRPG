#include "AI/BTTask_EnemySkill.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/BossCharacter.h"
#include "Character/EnemyCharacter.h"
#include "Data/BossDataAsset.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"

UBTTask_EnemySkill::UBTTask_EnemySkill()
{
    NodeName = "Perform Enemy Skill";
    bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_EnemySkill::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController) return EBTNodeResult::Failed;

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIController->GetPawn());
    if (!Enemy) return EBTNodeResult::Failed;

    ABossCharacter* Boss = Cast<ABossCharacter>(Enemy);
    if (Boss && Boss->bIsTransitioningPhase)
    {
        return EBTNodeResult::Failed;
    }

    // 확률 검사 (확률에 못 미치면 스킬 사용 안 함)
    if (ActivateProbability < 1.0f && FMath::FRand() > ActivateProbability)
    {
        return EBTNodeResult::Failed;
    }

    // 거리 및 정면 각도 검사
    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        if (AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName)))
        {
            float Distance = FVector::Dist(Enemy->GetActorLocation(), TargetActor->GetActorLocation());
            if (Distance < MinDistance || Distance > MaxDistance)
            {
                return EBTNodeResult::Failed;
            }

            // 정면 각도 검사 (MaxFacingAngle 이 180도 미만일 때 작동)
            if (MaxFacingAngle < 180.0f)
            {
                FVector Forward = Enemy->GetActorForwardVector().GetSafeNormal2D();
                FVector DirToTarget = (TargetActor->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
                float Dot = FVector::DotProduct(Forward, DirToTarget);
                float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));

                if (AngleDeg > MaxFacingAngle)
                {
                    // 정면 허용 각도를 벗어났으므로 스킬 발동 실패 -> 비헤이비어 트리가 Turn 회전 노드로 넘어가도록 유도!
                    return EBTNodeResult::Failed;
                }
            }
        }
    }

    UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
    if (!ASC) return EBTNodeResult::Failed;

    // 보스인 경우 BossData 기반으로 스킬 후보 수집, 일반 몬스터인 경우 ASC의 모든 어빌리티 중 수집
    TArray<TSubclassOf<UGameplayAbility>> SkillCandidates;

    if (Boss && Boss->BossData)
    {
        int32 CurrentPhase = Boss->CurrentPhase;
        if (Boss->BossData->PhaseList.IsValidIndex(CurrentPhase - 1))
        {
            const FBossPhaseData& PhaseData = Boss->BossData->PhaseList[CurrentPhase - 1];
            for (TSubclassOf<UGameplayAbility> SkillClass : PhaseData.GrantedSkills)
            {
                if (!SkillClass) continue;

                if (CategoryTag.IsValid())
                {
                    const UGameplayAbility* AbilityCDO = SkillClass.GetDefaultObject();
                    if (AbilityCDO && AbilityCDO->GetAssetTags().HasTag(CategoryTag))
                    {
                        SkillCandidates.Add(SkillClass);
                    }
                }
                else
                {
                    SkillCandidates.Add(SkillClass);
                }
            }
        }
    }
    else
    {
        // 일반 몬스터: ASC에 부여된 어빌리티들 중 CategoryTag가 맞는 어빌리티 수집
        for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
        {
            if (!Spec.Ability) continue;

            if (CategoryTag.IsValid())
            {
                if (Spec.Ability->GetAssetTags().HasTag(CategoryTag))
                {
                    SkillCandidates.Add(Spec.Ability->GetClass());
                }
            }
            else
            {
                SkillCandidates.Add(Spec.Ability->GetClass());
            }
        }
    }

    if (SkillCandidates.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ [BTTask_EnemySkill] 사용 가능한 스킬 후보군이 0개입니다! (CategoryTag 필터링 확인 필요)"));
        return EBTNodeResult::Failed;
    }

    // 무작위 패턴 설정 시 셔플
    if (bRandomSkillSelection)
    {
        for (int32 i = SkillCandidates.Num() - 1; i > 0; --i)
        {
            int32 j = FMath::RandRange(0, i);
            SkillCandidates.Swap(i, j);
        }
    }

    AIController->ClearFocus(EAIFocusPriority::Gameplay);

    for (TSubclassOf<UGameplayAbility> SkillClass : SkillCandidates)
    {
        FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(SkillClass);
        if (!Spec)
        {
            continue;
        }

        if (ASC->TryActivateAbilityByClass(SkillClass, true))
        {
            // 어빌리티가 EndAbility 되는 그 0.001초 바로 그 순간 BTTask를 100% 즉각 완료 처리!
            TWeakObjectPtr<UBTTask_EnemySkill> WeakThis(this);
            TWeakObjectPtr<UBehaviorTreeComponent> WeakOwnerComp(&OwnerComp);

            AbilityEndedHandle = ASC->OnAbilityEnded.AddLambda([WeakThis, WeakOwnerComp, ASC](const FAbilityEndedData& EndedData)
            {
                if (WeakThis.IsValid() && WeakOwnerComp.IsValid())
                {
                    if (ASC)
                    {
                        ASC->OnAbilityEnded.Remove(WeakThis->AbilityEndedHandle);
                    }
                    WeakThis->FinishLatentTask(*WeakOwnerComp.Get(), EBTNodeResult::Succeeded);
                }
            });

            return EBTNodeResult::InProgress;
        }
    }

    return EBTNodeResult::Failed;
}

void UBTTask_EnemySkill::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

    AAIController* AIC = OwnerComp.GetAIOwner();
    APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;
    IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Pawn);
    UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;

    if (ASC)
    {
        bool bHasAttackingTag = ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false));
        if (!bHasAttackingTag)
        {
            if (AbilityEndedHandle.IsValid())
            {
                ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
                AbilityEndedHandle.Reset();
            }
            FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        }
    }
}
