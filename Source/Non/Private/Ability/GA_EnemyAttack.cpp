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

  // 점프 공격 모션 워핑용 LocationTarget C++ 자동 주입!
  UpdateWarpTargetLocation();

  // 1. 몽타주 선택 (GA 내부 프로퍼티 사용)
  UAnimMontage *MontageToPlay = nullptr;
  if (AttackMontages.Num() > 0) {
    int32 Index = FMath::RandRange(0, AttackMontages.Num() - 1);
    MontageToPlay = AttackMontages[Index];
  }

  if (!MontageToPlay) {
    // 몽타주가 없으면 즉시 종료
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    return;
  }

  // 1.5. Motion Warping 타겟 좌표 전달 (LocationTarget) 및 회전 잠금
  if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
  {
      AIC->ClearFocus(EAIFocusPriority::Gameplay); // 공격 중 AI 시선 고정 해제 (엎어졌을 때 회전 차단)
  }

  if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
  {
      MoveComp->bOrientRotationToMovement = false; // 공격 중 이동 방향 회전 잠금
  }

  // 1.5. Motion Warping 타겟 좌표 1차 전달 (어빌리티 시작 시점)
  UpdateWarpTargetLocation();

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
        if (bWarpToTargetActor)
        {
            AActor* TargetActor = nullptr;
            if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
            {
                if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
                {
                    TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
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
        }
        else
        {
            // 플레이어 위치와 상관없이 보스 전방 방향 고정 거리(JumpForwardDistance cm) 점프!
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
