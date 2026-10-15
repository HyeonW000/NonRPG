#include "AI/BTTask_EnemySkill.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/BossCharacter.h"
#include "Character/EnemyCharacter.h"
#include "Data/BossDataAsset.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "Engine/Engine.h"
#include "Navigation/PathFollowingComponent.h"
#include "Combat/CombatSlotComponent.h"

// 타겟 플레이어에게 CombatSlotComponent가 있으면 슬롯 위치로, 없으면 액터 중심으로 이동
static void MoveToTargetOrSlot(AAIController* AIC, AActor* TargetActor, AEnemyCharacter* Enemy, float ApproachRadius)
{
    if (!AIC || !TargetActor || !Enemy) return;

    if (UCombatSlotComponent* SlotComp = TargetActor->FindComponentByClass<UCombatSlotComponent>())
    {
        SlotComp->ReserveSlot(Enemy);
        const FVector SlotPos = SlotComp->GetSlotLocation(Enemy, ApproachRadius);
        AIC->MoveToLocation(SlotPos, 25.0f, false);
    }
    else
    {
        AIC->MoveToActor(TargetActor, ApproachRadius, false);
    }
}

UBTTask_EnemySkill::UBTTask_EnemySkill()
{
    NodeName = "Perform Enemy Skill";
    bNotifyTick = true;
    bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_EnemySkill::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController) return EBTNodeResult::Failed;

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIController->GetPawn());
    if (!Enemy || Enemy->IsDead()) return EBTNodeResult::Failed;

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

    // 대상 액터 검색
    AActor* TargetActor = nullptr;
    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        FName KeyName = TargetActorKey.SelectedKeyName.IsNone() ? FName(TEXT("TargetActor")) : TargetActorKey.SelectedKeyName;
        TargetActor = Cast<AActor>(BB->GetValueAsObject(KeyName));
    }
    if (!TargetActor && Enemy->GetWorld())
    {
        if (APlayerController* PC = Enemy->GetWorld()->GetFirstPlayerController())
        {
            TargetActor = PC->GetPawn();
        }
    }

    if (!TargetActor)
    {
        return EBTNodeResult::Failed;
    }

    TargetActorPtr = TargetActor;

    const float Distance = FVector::Dist(Enemy->GetActorLocation(), TargetActor->GetActorLocation());

    // 몬스터 본체(DT_EnemyData)의 AttackRange를 실시간 반영!
    float EffectiveMaxDist = MaxDistance;
    if (Enemy->AttackRange > 0.f)
    {
        EffectiveMaxDist = FMath::Min(MaxDistance, Enemy->AttackRange);
    }
    CachedEffectiveMaxDist = EffectiveMaxDist;

    if (Distance < MinDistance)
    {
        return EBTNodeResult::Failed;
    }

    // ★ [히스테리시스 버퍼]: 160cm 경계선에서 캡슐 미세 떨림/밀림으로 1~2cm 벗어나도 타이머가 리셋되지 않도록 방지
    const float ResetDistance = EffectiveMaxDist + 70.0f;

    // 사정거리 밖이지만 전투 교전 거리(450cm) 이내인 경우: 멍때리지 않고 사정거리 안으로 부드럽게 접근(Approach)!
    if (Distance > EffectiveMaxDist)
    {
        // 플레이어가 사정거리 밖으로 완전히 멀어졌을 때만(230cm 초과) 뜸들이기(Windup) 타이머 리셋!
        if (Distance > ResetDistance)
        {
            Enemy->EnterRangeTime = -1.f;
        }

        if (Distance <= 450.f)
        {
            // 포위 슬롯이 있으면 슬롯 위치로, 없으면 액터 중심으로 이동
            const float ApproachRadius = FMath::Max(50.0f, EffectiveMaxDist * 0.55f);
            MoveToTargetOrSlot(AIController, TargetActor, Enemy, ApproachRadius);
            LastMoveRequestTime = Enemy->GetWorld() ? Enemy->GetWorld()->GetTimeSeconds() : 0.f;
            bIsApproachingTarget = true;
            return EBTNodeResult::InProgress;
        }
        else
        {
            // 450cm 초과면 원거리 추적 브랜치(Sequence_MoveToTarget)에 위임하기 위해 실패 반환
            return EBTNodeResult::Failed;
        }
    }

    // 사정거리 이내인 경우:
    // 1) 공격 쿨다운 중이면 대기
    if (!Enemy->IsAttackAllowed())
    {
        bIsApproachingTarget = false;
        return EBTNodeResult::InProgress;
    }

    // 2) 첫 진입 뜸들이기(Windup) 대기
    if (Enemy->EnterRangeTime < 0.f)
    {
        Enemy->MarkEnteredAttackRange();
        UE_LOG(LogTemp, Warning, TEXT("[AI 디버그] %s | 사정거리 진입! 뜸들이기(Windup) 시작: %.2f초"), *Enemy->GetName(), Enemy->CurrentWindupTime);
    }
    if (!Enemy->IsFirstAttackWindupDone())
    {
        bIsApproachingTarget = false;
        return EBTNodeResult::InProgress; // 틱에서 뜸들이기 완료까지 대기
    }

    // 뜸들이기 완료 후 즉시 공격 발동!
    Enemy->EnterRangeTime = -1.f;
    bIsApproachingTarget = false;
    return TryActivateSkill(OwnerComp, Enemy, TargetActor);
}

EBTNodeResult::Type UBTTask_EnemySkill::TryActivateSkill(UBehaviorTreeComponent& OwnerComp, AEnemyCharacter* Enemy, AActor* TargetActor)
{
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController || !Enemy || !TargetActor) return EBTNodeResult::Failed;

    ABossCharacter* Boss = Cast<ABossCharacter>(Enemy);

    // ★ [핵심] 공격 전 플레이어(타겟)를 정면으로 바라보도록 회전 보정 먼저 수행!
    FVector DirToTarget = (TargetActor->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
    if (!DirToTarget.IsNearlyZero())
    {
        FRotator TargetRot = DirToTarget.Rotation();
        TargetRot.Pitch = 0.0f;
        TargetRot.Roll = 0.0f;
        Enemy->SetActorRotation(TargetRot);
    }

    AIController->ClearFocus(EAIFocusPriority::Gameplay);

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
                    const bool bMatchesAssetTag = AbilityCDO && AbilityCDO->GetAssetTags().HasTag(CategoryTag);

                    if (bMatchesAssetTag)
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
        const FGameplayTag DefaultAttackTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack"), false);
        const bool bIsMeleeCategory = CategoryTag.IsValid() && (CategoryTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.Melee"), false)) || CategoryTag.MatchesTag(DefaultAttackTag));

        for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
        {
            if (!Spec.Ability) continue;

            if (CategoryTag.IsValid())
            {
                const bool bMatchesAssetTag = Spec.Ability->GetAssetTags().HasTag(CategoryTag);
                const bool bFallbackMeleeMatch = bIsMeleeCategory && Spec.Ability->GetAssetTags().HasTag(DefaultAttackTag);

                if (bMatchesAssetTag || bFallbackMeleeMatch)
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
        UE_LOG(LogTemp, Warning, TEXT("⚠️ [BTTask_EnemySkill] %s -> 사용 가능한 스킬 후보군이 0개입니다!"), *Enemy->GetName());
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

    for (TSubclassOf<UGameplayAbility> SkillClass : SkillCandidates)
    {
        FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(SkillClass);
        if (!Spec)
        {
            continue;
        }

        if (ASC->TryActivateAbilityByClass(SkillClass, true))
        {
            TWeakObjectPtr<UBTTask_EnemySkill> WeakThis(this);
            TWeakObjectPtr<UBehaviorTreeComponent> WeakOwnerComp(&OwnerComp);

            AbilityEndedHandle = ASC->OnAbilityEnded.AddLambda([WeakThis, WeakOwnerComp, ASC](const FAbilityEndedData& EndedData)
            {
                if (WeakThis.IsValid() && WeakOwnerComp.IsValid())
                {
                    if (ASC && WeakThis->AbilityEndedHandle.IsValid())
                    {
                        ASC->OnAbilityEnded.Remove(WeakThis->AbilityEndedHandle);
                        WeakThis->AbilityEndedHandle.Reset();
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
    if (!AIC)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIC->GetPawn());
    if (!Enemy || Enemy->IsDead())
    {
        if (bIsApproachingTarget)
        {
            AIC->StopMovement();
            bIsApproachingTarget = false;
        }
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    AActor* TargetActor = TargetActorPtr.Get();
    if (!TargetActor)
    {
        AIC->StopMovement();
        bIsApproachingTarget = false;
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    const float CurDist = FVector::Dist(Enemy->GetActorLocation(), TargetActor->GetActorLocation());
    const float EffectiveMaxDist = CachedEffectiveMaxDist;
    const float Now = Enemy->GetWorld() ? Enemy->GetWorld()->GetTimeSeconds() : 0.f;

    // 각도 계산
    FVector Forward = Enemy->GetActorForwardVector().GetSafeNormal2D();
    FVector DirToTarget = (TargetActor->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
    float Dot = FVector::DotProduct(Forward, DirToTarget);
    float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));

    // 실시간 디버그 메시지 (화면 출력 및 콘솔 출력)
    if (GEngine)
    {
        const FString StateStr = bIsApproachingTarget ? TEXT("APPROACHING (거리 좁히는 중)") : TEXT("ATTACKING / READY");
        const FColor StateColor = bIsApproachingTarget ? FColor::Cyan : FColor::Green;
        const FString DebugMsg = FString::Printf(
            TEXT("[AI 디버그] %s | 거리: %.1f cm | 사정거리: %.1f cm | 각도: %.1f° | 상태: %s"),
            *Enemy->GetName(), CurDist, EffectiveMaxDist, AngleDeg, *StateStr
        );
        GEngine->AddOnScreenDebugMessage((uint64)Enemy->GetUniqueID(), 0.25f, StateColor, DebugMsg);
    }

    // 1) 사정거리 접근 중인 상태
    if (bIsApproachingTarget)
    {
        // 사정거리 안으로 도달했거나, 다수 몬스터 충돌/정체로 인해 사정거리 근처(오차 35cm 이내)에서 멈춘 경우 -> 공격 발동!
        const bool bWithinAttackRange = (CurDist <= EffectiveMaxDist);
        const bool bCloseEnoughAndStopped = (CurDist <= (EffectiveMaxDist + 35.0f)) && (AIC->GetMoveStatus() != EPathFollowingStatus::Moving);

        if (bWithinAttackRange || bCloseEnoughAndStopped)
        {
            AIC->StopMovement();
            bIsApproachingTarget = false;

            // 사정거리 첫 진입 시 뜸들이기 타이머 마킹
            if (Enemy->EnterRangeTime < 0.f)
            {
                Enemy->MarkEnteredAttackRange();
                UE_LOG(LogTemp, Warning, TEXT("[AI 디버그] %s | 사정거리 진입! 뜸들이기(Windup) 시작: %.2f초"), *Enemy->GetName(), Enemy->CurrentWindupTime);
            }

            // 뜸들이기(Windup) 대기 중이면 아직 공격하지 않고 대기
            if (!Enemy->IsFirstAttackWindupDone())
            {
                return;
            }

            if (Enemy->IsAttackAllowed())
            {
                Enemy->EnterRangeTime = -1.f;
                UE_LOG(LogTemp, Warning, TEXT("[AI 디버그] %s | ★ 공격 발동! (거리: %.1f cm | 사정거리: %.1f cm)"), *Enemy->GetName(), CurDist, EffectiveMaxDist);
                EBTNodeResult::Type Result = TryActivateSkill(OwnerComp, Enemy, TargetActor);
                if (Result == EBTNodeResult::Failed)
                {
                    FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
                }
                else if (Result == EBTNodeResult::Succeeded)
                {
                    FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
                }
            }
            return;
        }

        // 플레이어가 500cm 초과로 너무 멀리 달아난 경우 -> 급정거(StopMovement)하지 않고 BT의 원거리 추적 브랜치로 부드럽게 인계!
        if (CurDist > 500.f)
        {
            Enemy->EnterRangeTime = -1.f;
            bIsApproachingTarget = false;
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            return;
        }

        // 포위 슬롯 위치로 분산 이동
        const float ApproachRadius = FMath::Max(50.0f, EffectiveMaxDist * 0.55f);
        if (AIC->GetMoveStatus() != EPathFollowingStatus::Moving || (Now - LastMoveRequestTime) > 0.35f)
        {
            MoveToTargetOrSlot(AIC, TargetActor, Enemy, ApproachRadius);
            LastMoveRequestTime = Now;
        }

        // 이동 중에도 타겟을 향해 부드럽게 회전 유지
        if (!DirToTarget.IsNearlyZero())
        {
            FRotator TargetRot = DirToTarget.Rotation();
            TargetRot.Pitch = 0.f;
            TargetRot.Roll = 0.f;
            FRotator NewRot = FMath::RInterpConstantTo(Enemy->GetActorRotation(), TargetRot, DeltaSeconds, 360.f);
            Enemy->SetActorRotation(NewRot);
        }

        return;
    }

    // 2) 어빌리티 실행 중이거나 사정거리 내 뜸들이기/쿨다운 대기 상태
    UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
    if (ASC)
    {
        bool bHasAttackingTag = ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false));
        if (!bHasAttackingTag)
        {
            // 뜸들이기(Windup) 대기 중인지 검사
            if (!Enemy->IsFirstAttackWindupDone())
            {
                // 뜸들이는 동안 플레이어를 똑바로 응시하도록 정면 회전 유지
                if (!DirToTarget.IsNearlyZero())
                {
                    FRotator TargetRot = DirToTarget.Rotation();
                    TargetRot.Pitch = 0.f;
                    TargetRot.Roll = 0.f;
                    Enemy->SetActorRotation(TargetRot);
                }

                const float RemainingWindup = FMath::Max(0.0f, Enemy->CurrentWindupTime - (Now - Enemy->EnterRangeTime));
                if (GEngine)
                {
                    const FString DebugMsg = FString::Printf(
                        TEXT("[AI 디버그] %s | 거리: %.1f cm | 사정거리: %.1f cm | 상태: WINDUP (공격 전 뜸들이기 대기: %.2f초)"),
                        *Enemy->GetName(), CurDist, EffectiveMaxDist, RemainingWindup
                    );
                    GEngine->AddOnScreenDebugMessage((uint64)Enemy->GetUniqueID(), 0.25f, FColor::Yellow, DebugMsg);
                }
                return;
            }

            // ★ [히스테리시스 버퍼]
            // 공격 발동 허용 범위: EffectiveMaxDist + 25.0f (예: 160 + 25 = 185cm)
            // 뜸들이기가 끝난 상태라면 1~2cm 경계선에서 살짝 벗어나도(185cm 이내) 근접 공격 모션 리치로 충분히 닿으므로 즉시 공격 개시!
            const float AttackTriggerDist = EffectiveMaxDist + 25.0f;
            const float ResetDist = EffectiveMaxDist + 70.0f;

            if (CurDist <= AttackTriggerDist && Enemy->IsAttackAllowed())
            {
                Enemy->EnterRangeTime = -1.f;
                UE_LOG(LogTemp, Warning, TEXT("[AI 디버그] %s | ★ 공격 발동! (거리: %.1f cm | 사정거리: %.1f cm)"), *Enemy->GetName(), CurDist, EffectiveMaxDist);
                EBTNodeResult::Type Result = TryActivateSkill(OwnerComp, Enemy, TargetActor);
                if (Result == EBTNodeResult::Failed)
                {
                    FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
                }
                else if (Result == EBTNodeResult::Succeeded)
                {
                    FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
                }
                return;
            }

            // 플레이어가 완전히 멀어진 경우(230cm 초과 ~ 500cm 이하): 추적 접근으로 전환하고 뜸들이기 리셋
            if (CurDist > ResetDist && CurDist <= 500.f)
            {
                Enemy->EnterRangeTime = -1.f;
                const float ApproachRadius = FMath::Max(50.0f, EffectiveMaxDist * 0.55f);
                MoveToTargetOrSlot(AIC, TargetActor, Enemy, ApproachRadius);
                LastMoveRequestTime = Now;
                bIsApproachingTarget = true;
                return;
            }

            // 버퍼 구간(185cm ~ 230cm)에 있는 경우:
            // 뜸들이기 타이머를 리셋하지 않고 살짝 거리를 좁히도록 접근
            if (CurDist > AttackTriggerDist && CurDist <= ResetDist)
            {
                const float ApproachRadius = FMath::Max(50.0f, EffectiveMaxDist * 0.55f);
                MoveToTargetOrSlot(AIC, TargetActor, Enemy, ApproachRadius);
                LastMoveRequestTime = Now;
                bIsApproachingTarget = true;
                return;
            }

            // 공격 쿨다운 등으로 아직 공격을 못하는 경우: 플레이어를 바라보며 대기
            if (!Enemy->IsAttackAllowed())
            {
                if (!DirToTarget.IsNearlyZero())
                {
                    FRotator TargetRot = DirToTarget.Rotation();
                    TargetRot.Pitch = 0.f;
                    TargetRot.Roll = 0.f;
                    Enemy->SetActorRotation(TargetRot);
                }
                return;
            }

            if (AbilityEndedHandle.IsValid())
            {
                ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
                AbilityEndedHandle.Reset();
            }
            FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        }
    }
}

EBTNodeResult::Type UBTTask_EnemySkill::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (AIController)
    {
        AIController->StopMovement();
    }
    bIsApproachingTarget = false;
    TargetActorPtr.Reset();

    if (AIController)
    {
        if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIController->GetPawn()))
        {
            Enemy->EnterRangeTime = -1.f;
        }

        if (APawn* Pawn = AIController->GetPawn())
        {
            if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Pawn))
            {
                if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
                {
                    if (AbilityEndedHandle.IsValid())
                    {
                        ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
                        AbilityEndedHandle.Reset();
                    }
                }
            }
        }
    }

    return Super::AbortTask(OwnerComp, NodeMemory);
}
