#include "AI/BTTask_BossSkill.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/BossCharacter.h"
#include "Data/BossDataAsset.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"

UBTTask_BossSkill::UBTTask_BossSkill()
{
    NodeName = "Perform Boss Skill";
}

EBTNodeResult::Type UBTTask_BossSkill::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController) return EBTNodeResult::Failed;

    ABossCharacter* Boss = Cast<ABossCharacter>(AIController->GetPawn());
    if (!Boss) return EBTNodeResult::Failed;

    // 페이즈 전환(포효 중 등)에는 스킬 사용을 중단
    if (Boss->bIsTransitioningPhase) 
    {
        return EBTNodeResult::Failed;
    }

    // [New] 확률 검사 (확률에 못 미치면 스킬 사용 안 함)
    if (ActivateProbability < 1.0f && FMath::FRand() > ActivateProbability)
    {
        return EBTNodeResult::Failed;
    }

    // [New] 거리 검사
    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        if (AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName)))
        {
            float Distance = FVector::Dist(Boss->GetActorLocation(), TargetActor->GetActorLocation());
            if (Distance < MinDistance || Distance > MaxDistance)
            {
                return EBTNodeResult::Failed;
            }
        }
    }

    UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
    if (!ASC || !Boss->BossData) return EBTNodeResult::Failed;

    int32 CurrentPhase = Boss->CurrentPhase;
    // 인덱스 안전성 검사
    if (!Boss->BossData->PhaseList.IsValidIndex(CurrentPhase - 1)) return EBTNodeResult::Failed;

    const FBossPhaseData& PhaseData = Boss->BossData->PhaseList[CurrentPhase - 1];
    
    // 이 페이즈에 할당된 스킬 목록 모으기
    TArray<TSubclassOf<UGameplayAbility>> SkillCandidates = PhaseData.GrantedSkills;
    if (SkillCandidates.Num() == 0) return EBTNodeResult::Failed;

    // 무작위 패턴 설정 시 배열을 마구 섞습니다 (랜덤 패턴의 핵심 로직)
    if (bRandomSkillSelection)
    {
        for (int32 i = SkillCandidates.Num() - 1; i > 0; --i)
        {
            int32 j = FMath::RandRange(0, i);
            SkillCandidates.Swap(i, j);
        }
    }

    // 섞인 후보군 중에서 하나씩 발동을 시도합니다.
    // 쿨타임이나 사거리(GameplayAbility 내부에 정의) 조건이 안 맞으면 알아서 다음 스킬로 넘어갑니다.
    for (TSubclassOf<UGameplayAbility> SkillClass : SkillCandidates)
    {
        if (ASC->TryActivateAbilityByClass(SkillClass, true))
        {
            return EBTNodeResult::Succeeded;
        }
    }

    // 쿨타임, 코스트 부족 등 모든 스킬 발동 불가 상태
    return EBTNodeResult::Failed;
}
