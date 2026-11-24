#include "Ability/GA_BossSkillBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Character/EnemyCharacter.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "MotionWarpingComponent.h"
#include "GameplayTagsManager.h"

UGA_BossSkillBase::UGA_BossSkillBase()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 공격 중 보스에게 State.Attacking 상태 태그 부여
	FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
	if (AttackTag.IsValid())
	{
		ActivationOwnedTags.AddTag(AttackTag);
	}

	// 기본 태그를 Enemy.Skill.Melee 로 지정
	FGameplayTagContainer DefaultAssetTags;
	FGameplayTag MeleeTag = FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.Melee"), false);
	if (MeleeTag.IsValid())
	{
		DefaultAssetTags.AddTag(MeleeTag);
	}
	SetAssetTags(DefaultAssetTags);
}

const FGameplayTagContainer* UGA_BossSkillBase::GetCooldownTags() const
{
	TempCooldownTags.Reset();
	if (CooldownTag.IsValid())
	{
		TempCooldownTags.AddTag(CooldownTag);
	}
	else
	{
		// 쿨다운 태그가 지정되지 않았다면 어빌리티 클래스 이름 기반으로 자동 생성
		const FName ClassTagName = FName(*FString::Printf(TEXT("Ability.Cooldown.%s"), *GetClass()->GetName()));
		TempCooldownTags.AddTag(FGameplayTag::RequestGameplayTag(ClassTagName, false));
	}
	return &TempCooldownTags;
}

bool UGA_BossSkillBase::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (CooldownDuration <= 0.0f)
	{
		return true; // 쿨타임이 0초면 언제든 발동 가능
	}

	const FGameplayTagContainer* CDTags = GetCooldownTags();
	if (CDTags && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		if (ActorInfo->AbilitySystemComponent->HasAnyMatchingGameplayTags(*CDTags))
		{
			if (OptionalRelevantTags)
			{
				OptionalRelevantTags->AppendTags(*CDTags);
			}
			return false; // 쿨다운 태그가 남아있으므로 발동 불가!
		}
	}

	return true;
}

void UGA_BossSkillBase::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (CooldownDuration <= 0.0f || !ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	const FGameplayTagContainer* CDTags = GetCooldownTags();
	if (!CDTags || CDTags->IsEmpty()) return;

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	// C++ 타이머로 지정된 초(CooldownDuration) 동안 쿨다운 태그를 유지한 뒤 자동 제거!
	for (const FGameplayTag& Tag : *CDTags)
	{
		if (Tag.IsValid())
		{
			ASC->AddLooseGameplayTag(Tag);

			TWeakObjectPtr<UAbilitySystemComponent> WeakASC(ASC);
			FTimerHandle CDHandle;
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(CDHandle, [WeakASC, Tag]()
				{
					if (WeakASC.IsValid())
					{
						WeakASC->RemoveLooseGameplayTag(Tag);
					}
				}, CooldownDuration, false);
			}
		}
	}
}

void UGA_BossSkillBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get());
	if (!Enemy)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 0. 타겟(플레이어)을 정면으로 정확히 바라보도록 회전 보정
	AActor* TargetActor = nullptr;
	if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
	{
		if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
		{
			TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
		}
	}
	// 블랙보드에 대상이 없으면 복제되는 추격 대상으로 대체 (첫 플레이어로 대체하면 협동에서 항상 호스트를 노림)
	if (!TargetActor)
	{
		TargetActor = Enemy->GetAggroTarget();
	}

	const bool bHasTurnAnim = Enemy->IsBoss() && (Enemy->TurnLeftMontage != nullptr || Enemy->TurnRightMontage != nullptr);
	const bool bIsBackSkill = IsBackSkill(Enemy);

	if (TargetActor && !bHasTurnAnim && !bIsBackSkill)
	{
		FVector Forward = Enemy->GetActorForwardVector().GetSafeNormal2D();
		FVector DirToTarget = (TargetActor->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
		if (!DirToTarget.IsNearlyZero())
		{
			float Dot = FVector::DotProduct(Forward, DirToTarget);
			float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
			if (AngleDeg <= 75.0f)
			{
				FRotator TargetRot = DirToTarget.Rotation();
				TargetRot.Pitch = 0.0f;
				TargetRot.Roll = 0.0f;
				Enemy->SetActorRotation(TargetRot);
			}
		}
	}

	// 모션 워핑 타겟 갱신
	UpdateWarpTargetLocation();

	// 1. 몽타주 확인
	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_BossSkillBase] %s: 재생할 MontageToPlay가 지정되지 않았습니다!"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// AI 시선 고정 해제 및 회전 잠금
	if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
	{
		AIC->ClearFocus(EAIFocusPriority::Gameplay);
	}
	if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = false;
	}

	// 2. 몽타주 재생 태스크 실행
	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, MontageToPlay, PlayRate, NAME_None, false
	);

	if (Task)
	{
		Task->OnBlendOut.AddDynamic(this, &UGA_BossSkillBase::OnMontageEnded);
		Task->OnInterrupted.AddDynamic(this, &UGA_BossSkillBase::OnMontageCancelled);
		Task->OnCancelled.AddDynamic(this, &UGA_BossSkillBase::OnMontageCancelled);
		Task->OnCompleted.AddDynamic(this, &UGA_BossSkillBase::OnMontageEnded);
		Task->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UGA_BossSkillBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
		{
			MoveComp->bOrientRotationToMovement = true;
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_BossSkillBase::OnMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_BossSkillBase::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

bool UGA_BossSkillBase::IsBackSkill(const AEnemyCharacter* Enemy) const
{
	// 후방 공격: 이 어빌리티의 에셋 태그 또는 재생 몽타주의 스킬 태그가 Enemy.Skill.Back (이름 무관)
	static const FGameplayTag BackTag = FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.Back"), false);
	if (BackTag.IsValid() && GetAssetTags().HasTag(BackTag))
	{
		return true;
	}
	return Enemy && Enemy->IsBackAttackMontage(MontageToPlay);
}

void UGA_BossSkillBase::UpdateWarpTargetLocation()
{
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
	if (!Enemy) return;

	if (IsBackSkill(Enemy))
	{
		return;
	}

	if (UMotionWarpingComponent* MotionWarpingComp = Enemy->FindComponentByClass<UMotionWarpingComponent>())
	{
		AActor* TargetActor = nullptr;
		if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
		{
			if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
			{
				TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
			}
		}

		FVector TargetLoc = Enemy->GetActorLocation() + Enemy->GetActorForwardVector() * JumpForwardDistance;
		if (bWarpToTargetActor && TargetActor)
		{
			TargetLoc = TargetActor->GetActorLocation();
		}

		MotionWarpingComp->AddOrUpdateWarpTargetFromLocation(WarpTargetName, TargetLoc);
	}
}
