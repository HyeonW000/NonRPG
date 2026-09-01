#include "Ability/GA_EnemyTurn.h"
#include "Character/EnemyCharacter.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "MotionWarpingComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

UGA_EnemyTurn::UGA_EnemyTurn()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bRetriggerInstancedAbility = false;

    FGameplayTag PainTag = FGameplayTag::RequestGameplayTag(TEXT("State.Pain"), false);
    if (PainTag.IsValid())
    {
        CancelAbilitiesWithTag.AddTag(PainTag);
    }

    FGameplayTag BrokenTag = FGameplayTag::RequestGameplayTag(TEXT("State.Broken"), false);
    if (BrokenTag.IsValid())
    {
        CancelAbilitiesWithTag.AddTag(BrokenTag);
    }

    FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
    if (AttackTag.IsValid())
    {
        ActivationBlockedTags.AddTag(AttackTag); // 공격 중일 때는 회전 어빌리티 실행 절대 금지!
    }
}

void UGA_EnemyTurn::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AEnemyCharacter* Boss = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
    if (!Boss)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AActor* TargetActor = nullptr;
    if (AAIController* AIC = Cast<AAIController>(Boss->GetController()))
    {
        if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
        {
            TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
        }
    }

    if (!TargetActor)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 1. 상대 각도 계산
    float Angle = Boss->GetAngleToTarget(TargetActor);

    // 2. 각도 방향 판단 및 몽타주 선택 (양수: 오른쪽 회전, 음수: 왼쪽 회전)
    UAnimMontage* MontageToPlay = (Angle >= 0.0f) ? TurnRightMontage : TurnLeftMontage;

    if (!MontageToPlay)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 3. Motion Warping 회전값 설정 (1회 회전당 최대 ±90도로 제한하여 170도일 때 묵직하게 2회 회전 수행)
    UMotionWarpingComponent* MotionWarpingComp = Boss->FindComponentByClass<UMotionWarpingComponent>();
    if (MotionWarpingComp)
    {
        float ClampedAngle = FMath::Clamp(Angle, -90.0f, 90.0f);
        FRotator TargetRotation = Boss->GetActorRotation();
        TargetRotation.Yaw += ClampedAngle;
        TargetRotation.Pitch = 0.0f;
        TargetRotation.Roll = 0.0f;

        MotionWarpingComp->AddOrUpdateWarpTargetFromTransform(WarpTargetName, FTransform(TargetRotation, Boss->GetActorLocation()));
    }

    // 4. [Multiplayer Fix] GAS AbilityTask 를 통해 몽타주를 재생하여 클라이언트로 100% 자동 복제!
    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        NAME_None,
        MontageToPlay,
        1.0f,
        NAME_None,
        true
    );

    if (MontageTask)
    {
        MontageTask->OnCompleted.AddDynamic(this, &UGA_EnemyTurn::HandleMontageFinished);
        MontageTask->OnInterrupted.AddDynamic(this, &UGA_EnemyTurn::HandleMontageFinished);
        MontageTask->OnCancelled.AddDynamic(this, &UGA_EnemyTurn::HandleMontageFinished);
        MontageTask->OnBlendOut.AddDynamic(this, &UGA_EnemyTurn::HandleMontageFinished);
        MontageTask->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGA_EnemyTurn::HandleMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_EnemyTurn::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    AEnemyCharacter* Boss = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());

    if (Boss && Boss->GetMesh() && Boss->GetMesh()->GetAnimInstance())
    {
        UAnimInstance* AnimInst = Boss->GetMesh()->GetAnimInstance();
        if (TurnRightMontage && AnimInst->Montage_IsPlaying(TurnRightMontage))
        {
            AnimInst->Montage_Stop(0.1f, TurnRightMontage);
        }
        else if (TurnLeftMontage && AnimInst->Montage_IsPlaying(TurnLeftMontage))
        {
            AnimInst->Montage_Stop(0.1f, TurnLeftMontage);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_EnemyTurn::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted);
}
