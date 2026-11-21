#include "AI/BTTask_ActivateAbility.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbility.h"
#include "Character/EnemyCharacter.h"
#include "Character/BossCharacter.h"
#include "Data/BossDataAsset.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/Engine.h"

UBTTask_ActivateAbility::UBTTask_ActivateAbility()
{
    NodeName = TEXT("Activate Ability (C++)");
    bNotifyTick = true;
    bCreateNodeInstance = true;
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

    const FString AbilityClassName = AbilityClass->GetName();
    const bool bIsTurnAbility = AbilityClassName.Contains(TEXT("Turn"));
    const bool bIsBackAttackAbility = AbilityClassName.Contains(TEXT("BackAttack"));
    const bool bIsGrabAbility = AbilityClassName.Contains(TEXT("Grab"));

    if (ABossCharacter* Boss = Cast<ABossCharacter>(Pawn))
    {
        // 잡기(Grab) 어빌리티인 경우: 보스에 잡기 몽타주가 세팅되어 있지 않다면 실행하지 않고 패스!
        if (bIsGrabAbility && !Boss->GrabAttemptMontage)
        {
            return EBTNodeResult::Failed;
        }

        // 회전(Turn)이나 뒤치기(BackAttack)가 아닌 일반 전방 어빌리티(예: Grab)는 정면(32도 이내)일 때만 발동!
        if (!bIsTurnAbility && !bIsBackAttackAbility && (Boss->TurnLeftMontage != nullptr || Boss->TurnRightMontage != nullptr))
        {
            if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
            {
                if (AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor"))))
                {
                    const float RealTimeAbsAngle = FMath::Abs(Boss->GetAngleToTarget(TargetActor));
                    BB->SetValueAsFloat(TEXT("TargetAngle"), RealTimeAbsAngle);
                    BB->SetValueAsFloat(TEXT("Angle"), RealTimeAbsAngle);
                    if (RealTimeAbsAngle > 32.0f)
                    {
                        return EBTNodeResult::Failed;
                    }
                }
            }
        }
    }

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

    // ★ 보스 데이터 주도형 스킬 연동 (예: BT_Boss의 Sequence_BackAttack에서 GA_Boss_BackAttack_Base 호출 시)
    // GrantedSkills를 비우고 DA_Boss의 Skills 배열(Enemy.Skill.Back)에 넣었을 때도 자동으로 찾아 실행!
    if (!bSuccess)
    {
        if (ABossCharacter* Boss = Cast<ABossCharacter>(Pawn))
        {
            if (Boss->BossData && AbilityClass->GetName().Contains(TEXT("BackAttack")))
            {
                const int32 PhaseIdx = Boss->CurrentPhase - 1;
                if (Boss->BossData->PhaseList.IsValidIndex(PhaseIdx))
                {
                    const FGameplayTag BackTag = FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.Back"), false);
                    for (const FBossSkillEntry& Entry : Boss->BossData->PhaseList[PhaseIdx].Skills)
                    {
                        if (!Entry.Montage) continue;
                        const bool bIsBackSkill = (BackTag.IsValid() && Entry.SkillCategoryTag.MatchesTag(BackTag)) ||
                                                  Entry.SkillName.ToString().Contains(TEXT("Back"));
                        if (!bIsBackSkill) continue;

                        if (Boss->IsBossSkillOnCooldown(Entry.SkillName))
                        {
                            return EBTNodeResult::Failed;
                        }

                        Boss->SetBossSkillCooldown(Entry.SkillName, Entry.Cooldown);
                        Boss->TriggerMeleeAttackCooldown();
                        AIC->ClearFocus(EAIFocusPriority::Gameplay);

                        const FGameplayTag AttackingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
                        if (AttackingTag.IsValid() && !ASC->HasMatchingGameplayTag(AttackingTag))
                        {
                            ASC->AddLooseGameplayTag(AttackingTag);
                        }

                        if (UCharacterMovementComponent* MoveComp = Boss->GetCharacterMovement())
                        {
                            MoveComp->bOrientRotationToMovement = false;
                        }

                        Boss->Multicast_PlayBossSkillMontage(Entry.Montage, Entry.PlayRate);

                        ActiveOwnerComp = &OwnerComp;
                        ActiveEnemy = Boss;

                        if (USkeletalMeshComponent* MeshComp = Boss->GetMesh())
                        {
                            if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
                            {
                                FOnMontageEnded EndDelegate;
                                EndDelegate.BindUObject(this, &UBTTask_ActivateAbility::OnBossMontageEnded);
                                AnimInst->Montage_SetEndDelegate(EndDelegate, Entry.Montage);
                            }
                        }

                        return EBTNodeResult::InProgress;
                    }
                }
            }
        }

        // 만약 GA_EnemyTurn 등이 DefaultAbilities 누락으로 아직 ASC에 부여되지 않았다면 즉석에서 부여 후 실행!
        if (!AbilityClass->GetName().Contains(TEXT("BackAttack")) && Pawn->HasAuthority())
        {
            FGameplayAbilitySpecHandle NewHandle = ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, Pawn));
            if (ASC->TryActivateAbility(NewHandle))
            {
                AIC->ClearFocus(EAIFocusPriority::Gameplay);
                return EBTNodeResult::InProgress;
            }
        }

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

void UBTTask_ActivateAbility::OnBossMontageEnded(UAnimMontage* Montage, bool bInterrupted)
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
        FinishLatentTask(*ActiveOwnerComp.Get(), bInterrupted ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
        ActiveOwnerComp.Reset();
        ActiveEnemy.Reset();
    }
}

void UBTTask_ActivateAbility::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
    Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);

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
        ActiveEnemy.Reset();
    }
    ActiveOwnerComp.Reset();

    // 어빌리티 종료 시 블랙보드 각도/거리 즉시 갱신 (보스는 SetFocus로 무모션 강제 회전하지 않도록 보호)
    if (AAIController* AIC = OwnerComp.GetAIOwner())
    {
        AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIC->GetPawn());
        if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
        {
            if (AActor* Target = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor"))))
            {
                if (Enemy)
                {
                    const float Dist = FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation());
                    const float AbsAngle = FMath::Abs(Enemy->GetAngleToTarget(Target));
                    BB->SetValueAsFloat(TEXT("DistanceToTarget"), Dist);
                    BB->SetValueAsFloat(TEXT("Distance"), Dist);
                    BB->SetValueAsFloat(TEXT("TargetAngle"), AbsAngle);
                    BB->SetValueAsFloat(TEXT("Angle"), AbsAngle);

                    if (!Enemy->IsBoss() && !Enemy->TurnLeftMontage && !Enemy->TurnRightMontage)
                    {
                        AIC->SetFocus(Target);
                    }
                }
            }
        }
    }
}
