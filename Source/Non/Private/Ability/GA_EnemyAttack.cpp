#include "Ability/GA_EnemyAttack.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Character/EnemyCharacter.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "MotionWarpingComponent.h"

UGA_EnemyAttack::UGA_EnemyAttack() {
  NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
  ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
  InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

  FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
  if (AttackTag.IsValid())
  {
      ActivationOwnedTags.AddTag(AttackTag); // 공격 동안 캐릭터에게 State.Attacking 태그 자동 주입!
  }

  // 기본 태그를 Enemy.Skill.Melee 로 지정하여 BTTask_EnemySkill의 카테고리 태그와 완전 일치시킴
  FGameplayTagContainer DefaultAssetTags;
  FGameplayTag MeleeTag = FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.Melee"), false);
  if (MeleeTag.IsValid())
  {
      DefaultAssetTags.AddTag(MeleeTag);
  }
  SetAssetTags(DefaultAssetTags);
}

void UGA_EnemyAttack::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo *ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData *TriggerEventData) {
  if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    return;
  }

  AEnemyCharacter *Enemy = Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get());
  if (!Enemy) {
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
  if (!TargetActor && Enemy->GetWorld())
  {
      if (APlayerController* PC = Enemy->GetWorld()->GetFirstPlayerController())
      {
          TargetActor = PC->GetPawn();
      }
  }

  if (TargetActor)
  {
      FVector Forward = Enemy->GetActorForwardVector().GetSafeNormal2D();
      FVector DirToTarget = (TargetActor->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
      if (!DirToTarget.IsNearlyZero())
      {
          float Dot = FVector::DotProduct(Forward, DirToTarget);
          float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
          // 정면 시야각(75도 이내)일 때만 정밀 조준 회전 (뒤에 있을 때 180도 홱 도는 현상 원천 차단)
          if (AngleDeg <= 75.0f)
          {
              FRotator TargetRot = DirToTarget.Rotation();
              TargetRot.Pitch = 0.0f;
              TargetRot.Roll = 0.0f;
              Enemy->SetActorRotation(TargetRot);
          }
      }
  }

  // 점프 공격 모션 워핑용 LocationTarget C++ 자동 주입!
  UpdateWarpTargetLocation();

  // 1. 몽타주 선택: 어빌리티 자체에 CustomMontage가 지정되어 있으면 최우선 사용! 없으면 DT_EnemyData의 AttackMontages 사용
  UAnimMontage *MontageToPlay = CustomMontage;
  if (!MontageToPlay && Enemy->AttackMontages.Num() > 0) {
    int32 Index = FMath::RandRange(0, Enemy->AttackMontages.Num() - 1);
    MontageToPlay = Enemy->AttackMontages[Index];
  }

  if (!MontageToPlay) {
    UE_LOG(LogTemp, Warning, TEXT("[GA_EnemyAttack] %s: DT_EnemyData(또는 캐릭터)에 AttackMontages가 비어있습니다!"), *Enemy->GetName());
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    return;
  }

  // 1.5. Motion Warping 타겟 좌표 전달 (LocationTarget) 및 회전 잠금
  if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
  {
      AIC->ClearFocus(EAIFocusPriority::Gameplay); // 공격 중 AI 시선 고정 해제 (공격 애니메이션 도중 홱 도는 것 방지)
  }

  if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
  {
      MoveComp->bOrientRotationToMovement = false; // 공격 중 이동 방향 회전 잠금
  }

  // 2. 몽타주 재생
  UAbilityTask_PlayMontageAndWait *Task =
      UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
          this, NAME_None, MontageToPlay, 1.0f, NAME_None,
          false // bStopWhenAbilityEnds
      );

  if (Task) {
    Task->OnBlendOut.AddDynamic(this, &UGA_EnemyAttack::OnMontageEnded);
    Task->OnInterrupted.AddDynamic(this, &UGA_EnemyAttack::OnMontageCancelled);
    Task->OnCancelled.AddDynamic(this, &UGA_EnemyAttack::OnMontageCancelled);
    Task->OnCompleted.AddDynamic(this, &UGA_EnemyAttack::OnMontageEnded);
    Task->ReadyForActivation();
  } else {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
  }
}

void UGA_EnemyAttack::OnDynamicWarpTick()
{
    UpdateWarpTargetLocation();
}

void UGA_EnemyAttack::UpdateWarpTargetLocation()
{
    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
    if (!Enemy) return;

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
        if (!TargetActor && Enemy->GetWorld())
        {
            if (APlayerController* PC = Enemy->GetWorld()->GetFirstPlayerController())
            {
                TargetActor = PC->GetPawn();
            }
        }

        if (TargetActor)
        {
            FVector TargetLoc = TargetActor->GetActorLocation();
            FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(Enemy->GetActorLocation(), TargetLoc);
            TargetRot.Pitch = 0.0f;
            TargetRot.Roll = 0.0f;

            MotionWarpingComp->AddOrUpdateWarpTargetFromTransform(WarpTargetName, FTransform(TargetRot, TargetLoc));
        }
        else
        {
            FVector FixedWarpLoc = Enemy->GetActorLocation() + (Enemy->GetActorForwardVector() * JumpForwardDistance);
            FRotator FixedWarpRot = Enemy->GetActorRotation();

            MotionWarpingComp->AddOrUpdateWarpTargetFromTransform(WarpTargetName, FTransform(FixedWarpRot, FixedWarpLoc));
        }
    }
}

void UGA_EnemyAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DynamicWarpTrackingTimerHandle);
    }

    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo()))
    {
        if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
        {
            MoveComp->bOrientRotationToMovement = true; // 공격 끝나면 이동 회전 복구
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_EnemyAttack::OnMontageEnded() {
  EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true,
             false);
}

void UGA_EnemyAttack::OnMontageCancelled() {
  EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true,
             true);
}
