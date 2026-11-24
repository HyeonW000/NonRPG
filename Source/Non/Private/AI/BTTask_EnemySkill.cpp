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
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "Kismet/KismetMathLibrary.h"

// 타겟 플레이어에게 CombatSlotComponent가 있으면 슬롯 위치로, 없으면 액터 중심으로 이동
static void MoveToTargetOrSlot(AAIController* AIC, AActor* TargetActor, AEnemyCharacter* Enemy, float ApproachRadius)
{
    if (!AIC || !TargetActor || !Enemy) return;
    if (Enemy->IsFleeing()) return; // ★ 후퇴(카이팅) 이동 중에는 절대 접근 이동 명령으로 덮어쓰지 않음!

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
    // 블랙보드에 대상이 없으면 복제되는 추격 대상으로 대체 (첫 플레이어로 대체하면 협동에서 항상 호스트를 노림)
    if (!TargetActor)
    {
        TargetActor = Enemy->GetAggroTarget();
    }

    if (!TargetActor)
    {
        return EBTNodeResult::Failed;
    }

    TargetActorPtr = TargetActor;

    const float Distance = FVector::Dist(Enemy->GetActorLocation(), TargetActor->GetActorLocation());

    // 원거리 스킬 카테고리인지 판별
    const bool bIsRangeSkill = CategoryTag.IsValid() && (
        CategoryTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.RangeAttack"), false))
    );

    const bool bIsBoss = (Boss != nullptr) || Enemy->IsBoss();
    const bool bIsBackSkill = CategoryTag.IsValid() && (
        CategoryTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.Back"), false))
    );

    // ★ 보스(또는 회전 몽타주 보유 캐릭터)인 경우: 실시간 각도를 즉시 블랙보드에 동기화하고,
    // 정면(32도 이내)을 벗어났다면 무모션 스냅 회전 공격을 하지 않고 Sequence_TurnInPlace(GA_EnemyTurn) 또는 Sequence_BackAttack에 즉시 양보!
    // (단, 후방 공격(Back Attack)은 뒤에 있는 타겟을 공격하는 스킬이므로 정면 각도 검사 제외!)
    if (bIsBoss && (Enemy->TurnLeftMontage != nullptr || Enemy->TurnRightMontage != nullptr))
    {
        const float RealTimeAbsAngle = FMath::Abs(Enemy->GetAngleToTarget(TargetActor));
        if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
        {
            BB->SetValueAsFloat(TEXT("TargetAngle"), RealTimeAbsAngle);
            BB->SetValueAsFloat(TEXT("Angle"), RealTimeAbsAngle);
        }
        if (!bIsBackSkill && RealTimeAbsAngle > 32.0f)
        {
            return EBTNodeResult::Failed;
        }
    }

    // 0) 스타일 및 몽타주 유효성 원천 검사:
    // (보스는 AttackMontages 대신 PhaseData.Skills 표를 사용하므로 일반 몬스터의 AttackMontages 검사 제외!)
    if (!bIsBoss)
    {
        // 근접 스킬인데 원거리 전용 몬스터이거나 근접 몽타주가 없으면 실행 즉시 원천 차단!
        if (!bIsRangeSkill)
        {
            if (Enemy->CombatStyle == EEnemyCombatStyle::Ranged || Enemy->AttackMontages.Num() == 0)
            {
                return EBTNodeResult::Failed;
            }
        }
        // 원거리 스킬인데 근접 전용 몬스터이거나 원거리 몽타주가 없으면 실행 즉시 원천 차단!
        if (bIsRangeSkill)
        {
            if (Enemy->CombatStyle == EEnemyCombatStyle::Melee || !Enemy->RangeAttackMontage)
            {
                return EBTNodeResult::Failed;
            }
        }
    }
    else
    {
        // 보스인 경우: BossData가 유효하고 해당 페이즈에 현재 사용 가능한(쿨타임이 아닌) 스킬이 있는지 확인
        if (Boss && Boss->BossData)
        {
            int32 PhaseIdx = Boss->CurrentPhase - 1;
            if (Boss->BossData->PhaseList.IsValidIndex(PhaseIdx))
            {
                const FBossPhaseData& PhaseData = Boss->BossData->PhaseList[PhaseIdx];
                bool bHasReadySkill = false;

                for (const FBossSkillEntry& Entry : PhaseData.Skills)
                {
                    if (!Entry.Montage) continue;
                    const bool bTagMatch = !CategoryTag.IsValid() || (Entry.SkillCategoryTag.IsValid() && (Entry.SkillCategoryTag.MatchesTag(CategoryTag) || CategoryTag.MatchesTag(Entry.SkillCategoryTag)));
                    if (!bTagMatch) continue;

                    // 개별 스킬 쿨타임 중이면 제외 (해당 카테고리의 모든 스킬이 쿨타임이면 즉시 Failed 반환하여 하위 이동 노드로 넘어감!)
                    if (Boss->IsBossSkillOnCooldown(Entry.SkillName)) continue;

                    // 최소 사정거리(MinRange) 미만이면 제외 (예: 원거리 스킬 최소 사거리 안쪽이면 근접/이동을 수행하도록 패스)
                    if (Entry.MinRange > 0.0f && Distance < (Entry.MinRange - 60.0f)) continue;

                    bHasReadySkill = true;
                    break;
                }

                if (!bHasReadySkill)
                {
                    for (TSubclassOf<UGameplayAbility> SkillClass : PhaseData.GrantedSkills)
                    {
                        if (!SkillClass) continue;
                        if (CategoryTag.IsValid())
                        {
                            const UGameplayAbility* AbilityCDO = SkillClass.GetDefaultObject();
                            if (!AbilityCDO || !AbilityCDO->GetAssetTags().HasTag(CategoryTag)) continue;
                        }
                        bHasReadySkill = true;
                        break;
                    }
                }

                if (!bHasReadySkill)
                {
                    return EBTNodeResult::Failed;
                }
            }
        }
    }

    // 몬스터 본체(DT_EnemyData 또는 BossDataAsset)의 사정거리를 실시간 반영!
    float EffectiveMaxDist = MaxDistance;
    if (bIsBoss && Boss && Boss->BossData)
    {
        float MaxBossRange = 0.0f;
        int32 PhaseIdx = Boss->CurrentPhase - 1;
        if (Boss->BossData->PhaseList.IsValidIndex(PhaseIdx))
        {
            for (const FBossSkillEntry& Entry : Boss->BossData->PhaseList[PhaseIdx].Skills)
            {
                if (!Entry.Montage) continue;
                if (Boss->IsBossSkillOnCooldown(Entry.SkillName)) continue;
                if (!CategoryTag.IsValid() || (Entry.SkillCategoryTag.IsValid() && (Entry.SkillCategoryTag.MatchesTag(CategoryTag) || CategoryTag.MatchesTag(Entry.SkillCategoryTag))))
                {
                    MaxBossRange = FMath::Max(MaxBossRange, Entry.MaxRange);
                }
            }
        }
        EffectiveMaxDist = (MaxBossRange > 0.0f) ? MaxBossRange : ((Enemy->AttackRange > 0.f) ? Enemy->AttackRange : MaxDistance);
    }
    else if (bIsRangeSkill)
    {
        EffectiveMaxDist = (Enemy->RangeAttackRange > 0.f) ? Enemy->RangeAttackRange : MaxDistance;
    }
    else
    {
        EffectiveMaxDist = (Enemy->AttackRange > 0.f) ? Enemy->AttackRange : MaxDistance;
    }
    CachedEffectiveMaxDist = EffectiveMaxDist;

    const bool bIsPureRanged = !bIsBoss && ((Enemy->CombatStyle == EEnemyCombatStyle::Ranged) || (Enemy->AttackMontages.Num() == 0));
    const bool bIsHybrid = !bIsBoss && ((Enemy->CombatStyle == EEnemyCombatStyle::Hybrid) || (Enemy->RangeAttackMontage != nullptr && Enemy->AttackMontages.Num() > 0));

    // ★ [핵심: 후퇴(카이팅) 이동 중 최우선 보호]
    // 후퇴 이동이 진행 중일 때는 절대 근접 스킬로 분기하거나 MoveToTargetOrSlot을 호출해 후퇴를 취소하지 않도록 보호!
    if (Enemy->IsFleeing())
    {
        bIsApproachingTarget = false;
        if (!bIsRangeSkill && Enemy->RangeAttackMontage != nullptr)
        {
            // 하이브리드 몬스터가 후퇴 중일 때는 근접 스킬을 Failed 처리하여 원거리 스킬 태스크에서 후퇴 완료 후 즉시 원거리 공격으로 이어지게 함
            return EBTNodeResult::Failed;
        }
        return EBTNodeResult::InProgress;
    }

    // 순수 원거리/하이브리드 몬스터는 MinDistance 검사를 건너뛰어 사정거리 내 어디서든 작동 보장
    if (!bIsPureRanged && !bIsHybrid && Distance < MinDistance)
    {
        return EBTNodeResult::Failed;
    }

    // ★ [하이브리드 근접/원거리 스마트 분기]:
    // 근접 교전 돌입 거리(MeleeEngageDist: 기본 350cm) 안으로 들어왔거나, 이미 근접 콤보가 진행 중인 경우 근접 모드로 판정!
    const float MeleeThreshold = (Enemy->AttackRange > 0.f) ? Enemy->AttackRange : 220.0f;
    const float MeleeEngageDist = (Enemy->FleeTriggerDistance > 0.f) ? Enemy->FleeTriggerDistance : 350.0f;
    const bool bHybridInMelee = bIsHybrid && (!Enemy->bPendingHybridFlee) && (Distance <= MeleeEngageDist || Enemy->HybridMeleeAttackCount > 0);

    // 원거리 스킬 검사인데 플레이어가 근접 교전 거리(350cm) 안으로 들어왔다면, 원거리 스킬을 취소하고 근접 공격 브랜치로 즉각 인계!
    if (bIsRangeSkill && Enemy->AttackMontages.Num() > 0 && bHybridInMelee)
    {
        return EBTNodeResult::Failed;
    }

    // ★ [히스테리시스 버퍼]: 경계선에서 캡슐 미세 떨림으로 타이머가 리셋되지 않도록 방지
    const float ResetDistance = EffectiveMaxDist + 70.0f;

    // 사정거리 밖인 경우:
    if (Distance > EffectiveMaxDist)
    {
        // 플레이어가 사정거리 밖으로 완전히 멀어졌을 때만 뜸들이기(Windup) 타이머 리셋!
        if (Distance > ResetDistance)
        {
            Enemy->EnterRangeTime = -1.f;
        }

        // 근접 스킬인데 플레이어가 근접 타격 사정거리(EffectiveMaxDist) 밖에 있는 경우:
        if (!bIsRangeSkill)
        {
            // 순수 원거리이거나, 하이브리드인데 아직 근접 교전 거리(350cm) 밖인 경우 -> 근접 이동 취소하고 원거리 스킬에 위임!
            if (bIsPureRanged || (bIsHybrid && !bHybridInMelee))
            {
                return EBTNodeResult::Failed;
            }

            // 하이브리드가 근접 콤보 진행 중인데 플레이어가 550cm 밖으로 완전히 달아났다면 -> 근접 콤보 리셋 후 원거리로 복귀!
            if (bIsHybrid && Distance > 550.0f)
            {
                Enemy->HybridMeleeAttackCount = 0;
                return EBTNodeResult::Failed;
            }

            // 순수 근접 몬스터(450cm 이내) 또는 근접 모드 돌입한 하이브리드 몬스터: 사정거리 안으로 성큼 걸어붙음(Approach)!
            const float ApproachRadius = FMath::Max(50.0f, EffectiveMaxDist * 0.55f);
            MoveToTargetOrSlot(AIController, TargetActor, Enemy, ApproachRadius);
            LastMoveRequestTime = Enemy->GetWorld() ? Enemy->GetWorld()->GetTimeSeconds() : 0.f;
            bIsApproachingTarget = true;
            return EBTNodeResult::InProgress;
        }
        // 원거리 스킬인데 원거리 사정거리(1200cm) 밖인 경우: 원거리 사정거리 안(70% 지점)으로만 부드럽게 접근!
        else if (bIsRangeSkill)
        {
            const float ApproachRadius = FMath::Max(150.0f, EffectiveMaxDist * 0.70f);
            MoveToTargetOrSlot(AIController, TargetActor, Enemy, ApproachRadius);
            LastMoveRequestTime = Enemy->GetWorld() ? Enemy->GetWorld()->GetTimeSeconds() : 0.f;
            bIsApproachingTarget = true;
            return EBTNodeResult::InProgress;
        }
        else
        {
            // 사정거리 밖이면 다른 브랜치나 추적 이동에 위임
            return EBTNodeResult::Failed;
        }
    }

    // 사정거리 이내인 경우:
    // 1) 공격 쿨다운 중인지 검사
    const bool bIsCooldownActive = bIsRangeSkill ? !Enemy->IsRangeAttackAllowed() : !Enemy->IsMeleeAttackAllowed();
    if (bIsCooldownActive)
    {
        // 순수 원거리 몬스터, 원거리 교전 중인 하이브리드, 또는 이미 근접 사정거리 안에 들어온 하이브리드는
        // 쿨타임 중일 때 다른 이동 노드로 빠져나가 이상한 이동을 하지 않고 제자리 주시 대기!
        const bool bHoldPosition = (bIsRangeSkill && bIsPureRanged) ||
                                   (bIsRangeSkill && bIsHybrid && Distance > MeleeThreshold) ||
                                   (!bIsRangeSkill && bIsHybrid && Distance <= MeleeThreshold);

        if (bHoldPosition)
        {
            if (Enemy->bFleeWhenTooClose && ((bIsPureRanged && Distance <= Enemy->FleeTriggerDistance) || (bIsHybrid && Enemy->bPendingHybridFlee)))
            {
                if (Enemy->TryPerformBackstepOrFlee(TargetActor))
                {
                    bIsApproachingTarget = false;
                    return EBTNodeResult::InProgress;
                }
            }
            AIController->StopMovement();
            bIsApproachingTarget = false;
            return EBTNodeResult::InProgress; // 틱에서 대기하면서 쿨타임 끝나면 즉시 공격
        }

        bIsApproachingTarget = false;
        return EBTNodeResult::Failed;
    }

    // 순수 원거리 몬스터의 위험 거리 진입 시, 또는 하이브리드 몬스터가 목표 근접 공격 횟수를 모두 마쳤을 때만 후퇴 실행!
    if (bIsRangeSkill && Enemy->bFleeWhenTooClose && ((bIsPureRanged && Distance <= Enemy->FleeTriggerDistance) || (bIsHybrid && Enemy->bPendingHybridFlee)))
    {
        if (Enemy->TryPerformBackstepOrFlee(TargetActor))
        {
            bIsApproachingTarget = false;
            return EBTNodeResult::InProgress;
        }
    }

    // 2) 첫 진입 뜸들이기(Windup) 대기 (보스는 플레이어 진입 시 즉시 공격하여 위압감 유지)
    if (!bIsBoss)
    {
        if (Enemy->EnterRangeTime < 0.f)
        {
            Enemy->MarkEnteredAttackRange();
        }
        if (!Enemy->IsFirstAttackWindupDone())
        {
            bIsApproachingTarget = false;
            return EBTNodeResult::InProgress; // 틱에서 뜸들이기 완료까지 대기
        }
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

    // ★ [핵심] 일반 몬스터는 공격 전 플레이어(타겟)를 정면으로 바라보도록 회전 보정 수행!
    // 단, 전용 회전 몽타주(TurnLeft/RightMontage)가 있는 보스는 공격 직전에 각도를 강제로 휙 스냅(SetActorRotation)하지 않고
    // GA_EnemyTurn 회전 애니메이션과 공격 몽타주의 MotionWarping으로만 자연스럽게 회전!
    // (단, 후방 공격(Back Attack)은 등 뒤의 타겟을 때리는 공격이므로 뒤돌아보는 회전 완전 제외!)
    const bool bIsBackSkillInActivate = CategoryTag.IsValid() && (
        CategoryTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.Back"), false))
    );

    const bool bHasTurnAnim = (Boss != nullptr || Enemy->IsBoss()) && (Enemy->TurnLeftMontage != nullptr || Enemy->TurnRightMontage != nullptr);
    if (!bHasTurnAnim && !bIsBackSkillInActivate)
    {
        FVector DirToTarget = (TargetActor->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
        if (!DirToTarget.IsNearlyZero())
        {
            FRotator TargetRot = DirToTarget.Rotation();
            TargetRot.Pitch = 0.0f;
            TargetRot.Roll = 0.0f;
            Enemy->SetActorRotation(TargetRot);
        }
    }

    AIController->ClearFocus(EAIFocusPriority::Gameplay);

    UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
    if (!ASC) return EBTNodeResult::Failed;

    // 보스인 경우 BossData 기반으로 스킬 후보 수집, 일반 몬스터인 경우 ASC의 모든 어빌리티 중 수집
    TArray<TSubclassOf<UGameplayAbility>> SkillCandidates;

    // 🌟 1. [보스 데이터 주도형 스킬] PhaseData.Skills 확인 (블루프린트 생성 없이 데이터 에셋에서 바로 실행!)
    if (Boss && Boss->BossData)
    {
        int32 CurrentPhase = Boss->CurrentPhase;
        if (Boss->BossData->PhaseList.IsValidIndex(CurrentPhase - 1))
        {
            const FBossPhaseData& PhaseData = Boss->BossData->PhaseList[CurrentPhase - 1];

            TArray<FBossSkillEntry> ValidDataSkills;
            float TotalWeight = 0.0f;

            for (const FBossSkillEntry& Entry : PhaseData.Skills)
            {
                if (!Entry.Montage) continue;

                // 1) 카테고리 태그 매칭 검사
                if (CategoryTag.IsValid() && Entry.SkillCategoryTag.IsValid())
                {
                    if (!Entry.SkillCategoryTag.MatchesTag(CategoryTag) && !CategoryTag.MatchesTag(Entry.SkillCategoryTag))
                    {
                        continue;
                    }
                }

                // 2) 사정거리 검사 (2D/3D 캡슐 및 데코레이터 경계선 오차 버퍼 허용)
                if (TargetActor)
                {
                    const float Dist2D = FVector::Dist2D(Enemy->GetActorLocation(), TargetActor->GetActorLocation());
                    const float Dist3D = FVector::Dist(Enemy->GetActorLocation(), TargetActor->GetActorLocation());
                    const float MinAllowed = FMath::Max(0.0f, Entry.MinRange - 60.0f);
                    const float MaxAllowed = Entry.MaxRange + 80.0f;
                    if ((Dist2D < MinAllowed && Dist3D < MinAllowed) || (Dist2D > MaxAllowed && Dist3D > MaxAllowed))
                    {
                        continue;
                    }
                }

                // 3) 개별 스킬 쿨타임 검사
                if (Boss->IsBossSkillOnCooldown(Entry.SkillName))
                {
                    continue;
                }

                ValidDataSkills.Add(Entry);
                TotalWeight += FMath::Max(0.1f, Entry.Weight);
            }

            // 사용 가능한 데이터 주도형 스킬이 있다면 가중치 룰렛으로 선택하여 즉시 실행!
            if (ValidDataSkills.Num() > 0)
            {
                FBossSkillEntry ChosenSkill = ValidDataSkills[0];
                if (ValidDataSkills.Num() > 1)
                {
                    float RandomRoll = FMath::FRandRange(0.0f, TotalWeight);
                    float Accum = 0.0f;
                    for (const FBossSkillEntry& SkillEntry : ValidDataSkills)
                    {
                        Accum += FMath::Max(0.1f, SkillEntry.Weight);
                        if (RandomRoll <= Accum)
                        {
                            ChosenSkill = SkillEntry;
                            break;
                        }
                    }
                }

                // 보스 개별 스킬 쿨타임 등록
                Boss->SetBossSkillCooldown(ChosenSkill.SkillName, ChosenSkill.Cooldown);

                // 카테고리별 공용 쿨타임 타이머 트리거
                const bool bIsRangeSkill = CategoryTag.IsValid() && (
                    CategoryTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.RangeAttack"), false))
                );
                if (bIsRangeSkill)
                {
                    Enemy->TriggerRangeAttackCooldown();
                }
                else
                {
                    Enemy->TriggerMeleeAttackCooldown();
                }

                // ★ 공격 중 상태 태그(State.Attacking) 부여 -> TickTask에서 공격 도중 취소되거나 턴 어빌리티가 끼어드는 현상 차단!
                const FGameplayTag AttackingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
                if (AttackingTag.IsValid() && !ASC->HasMatchingGameplayTag(AttackingTag))
                {
                    ASC->AddLooseGameplayTag(AttackingTag);
                }

                // 공격 모션 중 이동 방향 자동 회전 잠금
                if (UCharacterMovementComponent* MoveComp = Boss->GetCharacterMovement())
                {
                    MoveComp->bOrientRotationToMovement = false;
                }

                // 후방 공격 여부는 스킬 카테고리 태그(Enemy.Skill.Back)로만 판정 (에셋/스킬 이름에 의존하지 않음)
                const bool bIsBackSkillEntry = ChosenSkill.SkillCategoryTag.IsValid() &&
                    ChosenSkill.SkillCategoryTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.Back"), false));

                // 모션 워핑(Motion Warping) 타겟 위치 및 회전 갱신 (서버 + 모든 클라이언트 동기화)
                // (단, 후방 공격(Back Attack)은 등 뒤의 타겟을 때리는 공격이므로 플레이어 방향으로 모션 워핑 회전 금지!)
                if (TargetActor && !bIsBackSkillEntry)
                {
                    Boss->UpdateBossMotionWarpTarget(TargetActor);
                }

                // ★ 멀티캐스트로 서버 + 모든 클라이언트에서 동시에 보스 스킬 몽타주 재생!
                Boss->Multicast_PlayBossSkillMontage(ChosenSkill.Montage, ChosenSkill.PlayRate);

                ActiveOwnerComp = &OwnerComp;
                ActiveEnemy = Enemy;

                // 몽타주 종료 델리게이트 바인딩 (몽타주 종료 시 자동으로 태스크 성공 반환!)
                if (USkeletalMeshComponent* MeshComp = Boss->GetMesh())
                {
                    if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
                    {
                        FOnMontageEnded EndDelegate;
                        EndDelegate.BindUObject(this, &UBTTask_EnemySkill::OnBossMontageEnded);
                        AnimInst->Montage_SetEndDelegate(EndDelegate, ChosenSkill.Montage);
                    }
                }

                return EBTNodeResult::InProgress;
            }

            // 2. 만약 데이터 스킬이 없거나 쿨타임 중이면, 기존 GrantedSkills 어빌리티 목록에서 수집
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
            // 스킬 발동 성공! 카테고리별 쿨다운 타이머 즉시 시작!
            const bool bIsRangeSkill = CategoryTag.IsValid() && (
                CategoryTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.RangeAttack"), false))
            );
            if (bIsRangeSkill)
            {
                Enemy->TriggerRangeAttackCooldown();
            }
            else
            {
                Enemy->TriggerMeleeAttackCooldown();
            }

            TWeakObjectPtr<UBTTask_EnemySkill> WeakThis(this);
            TWeakObjectPtr<UBehaviorTreeComponent> WeakOwnerComp(&OwnerComp);
            TWeakObjectPtr<AEnemyCharacter> WeakEnemy(Enemy);
            TWeakObjectPtr<AActor> WeakTarget(TargetActor);

            AbilityEndedHandle = ASC->OnAbilityEnded.AddLambda([WeakThis, WeakOwnerComp, WeakEnemy, WeakTarget, ASC, bIsRangeSkill](const FAbilityEndedData& EndedData)
            {
                if (WeakEnemy.IsValid())
                {
                    AEnemyCharacter* E = WeakEnemy.Get();
                    // ★ 공격 모션이 끝난 시점을 기준으로 쿨타임 재갱신 (공격 모션 도중 쿨타임이 다 소모되어 연속 공격이 나가는 문제 원천 방지)
                    if (bIsRangeSkill)
                    {
                        E->TriggerRangeAttackCooldown();
                        E->HybridMeleeAttackCount = 0;
                        E->bPendingHybridFlee = false;
                    }
                    else
                    {
                        E->TriggerMeleeAttackCooldown();

                        // ★ 하이브리드 몬스터가 근접 공격을 마친 직후: 랜덤 목표 횟수(1~3회)만큼 근접 공격을 채웠을 때만 후퇴(거리 벌리기) 발동!
                        const bool bIsHybridEnemy = (E->CombatStyle == EEnemyCombatStyle::Hybrid) || (E->RangeAttackMontage != nullptr && E->AttackMontages.Num() > 0);
                        if (bIsHybridEnemy && E->bFleeWhenTooClose && WeakTarget.IsValid())
                        {
                            if (E->HybridTargetMeleeCount <= 0)
                            {
                                E->HybridTargetMeleeCount = FMath::RandRange(1, 3);
                            }
                            E->HybridMeleeAttackCount++;

                            if (E->HybridMeleeAttackCount >= E->HybridTargetMeleeCount)
                            {
                                E->HybridMeleeAttackCount = 0;
                                E->HybridTargetMeleeCount = FMath::RandRange(1, 3);
                                E->bPendingHybridFlee = true;
                                E->LastFleeTime = -100.f;
                                E->TryPerformBackstepOrFlee(WeakTarget.Get());
                            }
                        }
                    }
                }

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
    const bool bIsBossInTick = Enemy->IsBoss() || Enemy->IsA<ABossCharacter>();
    const bool bIsPureRanged = !bIsBossInTick && ((Enemy->CombatStyle == EEnemyCombatStyle::Ranged) || (Enemy->AttackMontages.Num() == 0));
    const bool bIsHybrid = !bIsBossInTick && ((Enemy->CombatStyle == EEnemyCombatStyle::Hybrid) || (Enemy->RangeAttackMontage != nullptr && Enemy->AttackMontages.Num() > 0));
    const float MeleeThreshold = (Enemy->AttackRange > 0.f) ? Enemy->AttackRange : 220.0f;

    // ★ [핵심: 후퇴(카이팅) 이동 중 최우선 보호]
    // 후퇴 이동이 진행 중일 때는 bIsApproachingTarget 검사(StopMovement 호출)나 근접 전환, 재추적을 일절 수행하지 않고 후퇴에 전념!
    if (Enemy->IsFleeing())
    {
        bIsApproachingTarget = false;
        return;
    }

    // 각도 계산
    FVector Forward = Enemy->GetActorForwardVector().GetSafeNormal2D();
    FVector DirToTarget = (TargetActor->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
    float Dot = FVector::DotProduct(Forward, DirToTarget);
    float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));



    // 1) 사정거리 접근 중인 상태
    if (bIsApproachingTarget)
    {
        // 원거리 스킬 카테고리 여부
        const bool bIsRangeSkillInTick = CategoryTag.IsValid() && (
            CategoryTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.RangeAttack"), false))
        );

        // 하이브리드 몬스터: 근접 콤보 도중 플레이어가 550cm 밖으로 달아났거나 후퇴 대기 중일 때만 접근 취소!
        if (!bIsRangeSkillInTick && bIsHybrid && (CurDist > 550.0f || Enemy->bPendingHybridFlee))
        {
            Enemy->HybridMeleeAttackCount = 0;
            bIsApproachingTarget = false;
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            return;
        }

        // 사정거리 안으로 도달했거나, 다수 몬스터 충돌/정체로 인해 사정거리 근처(오차 35cm 이내)에서 멈춘 경우 -> 공격 발동!
        const bool bWithinAttackRange = (CurDist <= EffectiveMaxDist);
        const bool bCloseEnoughAndStopped = (CurDist <= (EffectiveMaxDist + 35.0f)) && (AIC->GetMoveStatus() != EPathFollowingStatus::Moving);

        if (bWithinAttackRange || bCloseEnoughAndStopped)
        {
            AIC->StopMovement();
            bIsApproachingTarget = false;

            // 보스가 접근 완료했을 때 플레이어가 정면(32도 이내)을 벗어났다면 먼저 제자리 회전(TurnInPlace)을 수행하도록 양보!
            if (bIsBossInTick && (Enemy->TurnLeftMontage != nullptr || Enemy->TurnRightMontage != nullptr) && AngleDeg > 32.0f)
            {
                if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
                {
                    BB->SetValueAsFloat(TEXT("TargetAngle"), AngleDeg);
                    BB->SetValueAsFloat(TEXT("Angle"), AngleDeg);
                }
                FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
                return;
            }

            // 사정거리 첫 진입 시 뜸들이기 타이머 마킹 (보스는 뜸들이기 없이 즉시 공격!)
            if (!bIsBossInTick)
            {
                if (Enemy->EnterRangeTime < 0.f)
                {
                    Enemy->MarkEnteredAttackRange();
                }

                // 뜸들이기(Windup) 대기 중이면 아직 공격하지 않고 대기
                if (!Enemy->IsFirstAttackWindupDone())
                {
                    return;
                }
            }

            const bool bIsAllowed = bIsRangeSkillInTick ? Enemy->IsRangeAttackAllowed() : Enemy->IsMeleeAttackAllowed();

            if (bIsAllowed)
            {
                Enemy->EnterRangeTime = -1.f;
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
            else
            {
                const bool bHoldInTick = (bIsRangeSkillInTick && bIsPureRanged) ||
                                         (bIsRangeSkillInTick && bIsHybrid && CurDist > MeleeThreshold) ||
                                         (!bIsRangeSkillInTick && bIsHybrid && CurDist <= MeleeThreshold);
                if (bHoldInTick)
                {
                    AIC->StopMovement();
                    bIsApproachingTarget = false;
                    return; // 사정거리 내 쿨다운 대기
                }
                FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            }
            return;
        }

        // 보스가 접근 이동 도중 플레이어가 측면/후방(45도 초과)으로 돌아갔다면 무모션 회전 대신 TurnInPlace 노드로 양보!
        if (bIsBossInTick && (Enemy->TurnLeftMontage != nullptr || Enemy->TurnRightMontage != nullptr) && AngleDeg > 45.0f)
        {
            AIC->StopMovement();
            bIsApproachingTarget = false;
            if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
            {
                BB->SetValueAsFloat(TEXT("TargetAngle"), AngleDeg);
                BB->SetValueAsFloat(TEXT("Angle"), AngleDeg);
            }
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            return;
        }

        const bool bIsRangeSkill = bIsRangeSkillInTick;
        const float MaxChaseDist = bIsRangeSkill ? (EffectiveMaxDist + 400.f) : 500.f;

        // 플레이어가 너무 멀리 달아난 경우 -> 급정거(StopMovement)하지 않고 BT의 원거리 추적 브랜치로 부드럽게 인계!
        if (CurDist > MaxChaseDist)
        {
            Enemy->EnterRangeTime = -1.f;
            bIsApproachingTarget = false;
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            return;
        }

        // 포위 슬롯 위치로 분산 이동 (원거리 몬스터는 사정거리의 70%인 약 850cm까지만 접근)
        const float ApproachRadius = bIsRangeSkill ? FMath::Max(150.0f, EffectiveMaxDist * 0.70f) : FMath::Max(50.0f, EffectiveMaxDist * 0.55f);
        if (AIC->GetMoveStatus() != EPathFollowingStatus::Moving || (Now - LastMoveRequestTime) > 0.35f)
        {
            MoveToTargetOrSlot(AIC, TargetActor, Enemy, ApproachRadius);
            LastMoveRequestTime = Now;
        }

        // 일반 몬스터만 이동 중 타겟을 향해 코드 회전 유지 (보스는 SetActorRotation으로 휙 돌지 않도록 제외!)
        if (!bIsBossInTick && !DirToTarget.IsNearlyZero())
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
            const bool bIsRangeSkillInWait = CategoryTag.IsValid() && (
                CategoryTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.RangeAttack"), false))
            );

            const float MeleeEngageDist = (Enemy->FleeTriggerDistance > 0.f) ? Enemy->FleeTriggerDistance : 350.0f;
            const bool bHybridInMeleeInTick = bIsHybrid && (!Enemy->bPendingHybridFlee) && (CurDist <= MeleeEngageDist || Enemy->HybridMeleeAttackCount > 0);

            // ★ 1. [하이브리드 자동 분기 - 근접 전환]: 원거리 스킬 검사인데 플레이어가 근접 교전 거리(350cm) 안으로 들어왔다면?
            // 원거리 스킬을 취소(Failed)하여 근접 공격 브랜치로 제어권을 즉각 인계!
            if (bIsRangeSkillInWait && Enemy->AttackMontages.Num() > 0 && bHybridInMeleeInTick)
            {
                FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
                return;
            }

            // ★ 2. [하이브리드 자동 분기 - 원거리 전환]: 근접 스킬 대기 중인데 플레이어가 550cm 밖으로 완전히 달아났다면?
            // 근접 콤보 리셋 후 즉시 취소(Failed)하여 원거리 공격 브랜치로 제어권을 인계!
            if (!bIsRangeSkillInWait && bIsHybrid && CurDist > 550.0f)
            {
                Enemy->HybridMeleeAttackCount = 0;
                FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
                return;
            }

            // ★ 3. [카이팅 최우선 검사]: 순수 원거리 몬스터의 위험 거리 진입 시, 또는 하이브리드 몬스터가 목표 근접 공격 횟수를 다 채웠을 때만 후퇴(거리 벌리기) 실행!
            if (Enemy->bFleeWhenTooClose &&
                ((bIsRangeSkillInWait && bIsPureRanged && CurDist <= Enemy->FleeTriggerDistance) || (bIsHybrid && Enemy->bPendingHybridFlee)))
            {
                if (Enemy->TryPerformBackstepOrFlee(TargetActor))
                {
                    bIsApproachingTarget = false;
                    return; // 후퇴 시작! 이번 프레임 즉각 뒤로 물러남!
                }
            }

            // ★ 보스(회전 애니메이션 보유)인 경우 대기 도중 각도가 30도 이상 벌어지면 무모션 회전하지 않고 즉시 TurnInPlace 노드로 제어권 인계!
            if (bIsBossInTick && (Enemy->TurnLeftMontage != nullptr || Enemy->TurnRightMontage != nullptr))
            {
                if (AngleDeg > 30.0f)
                {
                    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
                    {
                        BB->SetValueAsFloat(TEXT("TargetAngle"), AngleDeg);
                        BB->SetValueAsFloat(TEXT("Angle"), AngleDeg);
                    }
                    FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
                    return;
                }
            }

            // ★ 4. 쿨다운 검사: 아직 공격 쿨타임이 안 끝났다면 사정거리 내에서 플레이어를 응시하며 대기
            const bool bIsAttackReady = bIsRangeSkillInWait ? Enemy->IsRangeAttackAllowed() : Enemy->IsMeleeAttackAllowed();
            if (!bIsAttackReady)
            {
                if (!bIsBossInTick && !DirToTarget.IsNearlyZero())
                {
                    FRotator TargetRot = DirToTarget.Rotation();
                    TargetRot.Pitch = 0.f;
                    TargetRot.Roll = 0.f;
                    const float TurnRate = (Enemy->AimTurnRate > 0.0f) ? Enemy->AimTurnRate : 220.0f;
                    FRotator NewRot = FMath::RInterpConstantTo(Enemy->GetActorRotation(), TargetRot, DeltaSeconds, TurnRate);
                    Enemy->SetActorRotation(NewRot);
                }

                const bool bHoldWait = (bIsRangeSkillInWait && bIsPureRanged) ||
                                       (bIsRangeSkillInWait && bIsHybrid && CurDist > MeleeThreshold) ||
                                       (!bIsRangeSkillInWait && bIsHybrid && CurDist <= MeleeThreshold);
                if (bHoldWait)
                {
                    AIC->StopMovement();
                    bIsApproachingTarget = false;
                    return; // 원거리/하이브리드 몬스터는 쿨다운 동안 사정거리 내에서 조준 유지하며 대기
                }

                FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
                return;
            }

            // ★ 5. [뜸들이기(Windup / Aim Delay) 대기 검사]: 사정거리 첫 진입 or 카이팅 후 정지 시 DT_EnemyData의 FirstAttackWindup 타이머 시작!
            if (Enemy->EnterRangeTime < 0.f)
            {
                Enemy->MarkEnteredAttackRange();
            }

            if (!Enemy->IsFirstAttackWindupDone())
            {
                // 뜸들이는 동안 플레이어를 향해 부드럽고 차분하게 몸을 돌리며 조준 (보스는 GA_EnemyTurn 전용 애니메이션을 사용하므로 제외)
                if (!bIsBossInTick && !DirToTarget.IsNearlyZero())
                {
                    FRotator TargetRot = DirToTarget.Rotation();
                    TargetRot.Pitch = 0.f;
                    TargetRot.Roll = 0.f;
                    const float TurnRate = (Enemy->AimTurnRate > 0.0f) ? Enemy->AimTurnRate : 220.0f;
                    FRotator NewRot = FMath::RInterpConstantTo(Enemy->GetActorRotation(), TargetRot, DeltaSeconds, TurnRate);
                    Enemy->SetActorRotation(NewRot);
                }

                const float RemainingWindup = FMath::Max(0.0f, Enemy->CurrentWindupTime - (Now - Enemy->EnterRangeTime));

                return;
            }

            // ★ [정면 조준 완성도 검사] 일반 몬스터는 아직 플레이어 방향으로 몸을 충분히 돌리지 못했다면(오차 25도 이상) 자연스럽게 마저 회전할 때까지 대기
            if (!bIsBossInTick && !DirToTarget.IsNearlyZero())
            {
                FRotator TargetRot = DirToTarget.Rotation();
                TargetRot.Pitch = 0.f;
                TargetRot.Roll = 0.f;
                const float AngleDiff = FMath::Abs(FRotator::NormalizeAxis(TargetRot.Yaw - Enemy->GetActorRotation().Yaw));
                if (AngleDiff > 25.0f)
                {
                    const float TurnRate = (Enemy->AimTurnRate > 0.0f) ? Enemy->AimTurnRate : 220.0f;
                    FRotator NewRot = FMath::RInterpConstantTo(Enemy->GetActorRotation(), TargetRot, DeltaSeconds, TurnRate);
                    Enemy->SetActorRotation(NewRot);
                    return; // 정면을 향할 때까지 대기
                }
            }

            // ★ 6. 공격 발동 허용 범위 검사 (히스테리시스 버퍼)
            // 하이브리드 근접 공격은 정확히 EffectiveMaxDist 이내일 때만 발동하고, 순수 근접/원거리만 25cm 버퍼 허용
            const float AttackTriggerDist = (!bIsRangeSkillInWait && bIsHybrid) ? EffectiveMaxDist : (EffectiveMaxDist + 25.0f);
            const float ResetDist = EffectiveMaxDist + 70.0f;

            if (CurDist <= AttackTriggerDist)
            {
                Enemy->EnterRangeTime = -1.f;
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

            // ★ 7. 거리 벌어짐 보정: 하이브리드 몬스터가 근접 스킬 중 거리가 벌어지면 절대 쫓아가지 않고 즉시 원거리 스킬로 전환!
            const bool bIsRangeSkill = bIsRangeSkillInWait;
            if (!bIsRangeSkill && (bIsHybrid || Enemy->RangeAttackMontage != nullptr))
            {
                FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
                return;
            }

            const float MaxChaseDist = bIsRangeSkill ? (EffectiveMaxDist + 400.f) : 500.f;

            if (CurDist > ResetDist && CurDist <= MaxChaseDist)
            {
                Enemy->EnterRangeTime = -1.f;
                const float ApproachRadius = bIsRangeSkill ? FMath::Max(150.0f, EffectiveMaxDist * 0.70f) : FMath::Max(50.0f, EffectiveMaxDist * 0.55f);
                MoveToTargetOrSlot(AIC, TargetActor, Enemy, ApproachRadius);
                LastMoveRequestTime = Now;
                bIsApproachingTarget = true;
                return;
            }

            // 버퍼 구간에 있는 경우: 뜸들이기 타이머를 리셋하지 않고 살짝 거리를 좁히도록 접근
            if (CurDist > AttackTriggerDist && CurDist <= ResetDist)
            {
                const float ApproachRadius = bIsRangeSkill ? FMath::Max(150.0f, EffectiveMaxDist * 0.70f) : FMath::Max(50.0f, EffectiveMaxDist * 0.55f);
                MoveToTargetOrSlot(AIC, TargetActor, Enemy, ApproachRadius);
                LastMoveRequestTime = Now;
                bIsApproachingTarget = true;
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
    AEnemyCharacter* Enemy = AIController ? Cast<AEnemyCharacter>(AIController->GetPawn()) : nullptr;

    // ★ 후퇴(카이팅) 이동 중에는 태스크가 전환(Abort)되더라도 후퇴 이동을 멈추지 않도록 보호!
    if (AIController && (!Enemy || !Enemy->IsFleeing()))
    {
        AIController->StopMovement();
    }
    bIsApproachingTarget = false;
    TargetActorPtr.Reset();

    if (AIController)
    {
        if (Enemy)
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

    if (ActiveEnemy.IsValid())
    {
        if (UAbilitySystemComponent* ASC = ActiveEnemy->GetAbilitySystemComponent())
        {
            const FGameplayTag AttackingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
            if (AttackingTag.IsValid())
            {
                ASC->RemoveLooseGameplayTag(AttackingTag);
            }
        }
        if (UCharacterMovementComponent* MoveComp = ActiveEnemy->GetCharacterMovement())
        {
            MoveComp->bOrientRotationToMovement = true;
        }
        if (USkeletalMeshComponent* Mesh = ActiveEnemy->GetMesh())
        {
            if (UAnimInstance* Anim = Mesh->GetAnimInstance())
            {
                Anim->Montage_Stop(0.2f);
            }
        }
        ActiveEnemy.Reset();
    }
    ActiveOwnerComp.Reset();

    return Super::AbortTask(OwnerComp, NodeMemory);
}

void UBTTask_EnemySkill::OnBossMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (ActiveEnemy.IsValid())
    {
        if (UAbilitySystemComponent* ASC = ActiveEnemy->GetAbilitySystemComponent())
        {
            const FGameplayTag AttackingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
            if (AttackingTag.IsValid())
            {
                ASC->RemoveLooseGameplayTag(AttackingTag);
            }
        }
        if (UCharacterMovementComponent* MoveComp = ActiveEnemy->GetCharacterMovement())
        {
            MoveComp->bOrientRotationToMovement = true;
        }
    }

    if (ActiveOwnerComp.IsValid())
    {
        if (ActiveEnemy.IsValid())
        {
            if (UBlackboardComponent* BB = ActiveOwnerComp->GetBlackboardComponent())
            {
                if (AActor* Target = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor"))))
                {
                    const float Dist = FVector::Dist2D(ActiveEnemy->GetActorLocation(), Target->GetActorLocation());
                    const float AbsAngle = FMath::Abs(ActiveEnemy->GetAngleToTarget(Target));
                    BB->SetValueAsFloat(TEXT("DistanceToTarget"), Dist);
                    BB->SetValueAsFloat(TEXT("Distance"), Dist);
                    BB->SetValueAsFloat(TEXT("TargetAngle"), AbsAngle);
                    BB->SetValueAsFloat(TEXT("Angle"), AbsAngle);
                }
            }
        }

        FinishLatentTask(*ActiveOwnerComp.Get(), bInterrupted ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
        ActiveOwnerComp.Reset();
        ActiveEnemy.Reset();
    }
}
