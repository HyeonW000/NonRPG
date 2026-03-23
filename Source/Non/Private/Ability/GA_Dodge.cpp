#include "Ability/GA_Dodge.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "Character/NonCharacterBase.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

UGA_Dodge::UGA_Dodge()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // [Fix] 멀티플레이어 동기화 필수 설정
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // [Fix] 하드코딩되었던 태그 설정들을 모두 철거했습니다.
    // 블루프린트(Class Defaults)에서 Trigger, OwnedTags, AbilityTags를 직접 지정해주세요.
}

void UGA_Dodge::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // Super::ActivateAbility(...) 

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ACharacter* Char = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Char)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // FullBody Set
    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Char))
    {
        NonChar->SetForceFullBody(true);
    }

    /* ===== 2) 방향 결정 (RPC 동기화 값 사용) ===== */
    EDodgeDirection Dir = EDodgeDirection::Forward;

    // [Fix] NonCharacterBase에 저장된 동기화된 방향 사용
    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Char))
    {
        Dir = (EDodgeDirection)NonChar->TempDodgeDirection;
    }
    else
    {
        // Fallback
        FVector MoveInput = Char->GetLastMovementInputVector();
        if (MoveInput.IsNearlyZero())
        {
            if (Char->GetVelocity().SizeSquared2D() > 10.f) MoveInput = Char->GetVelocity().GetSafeNormal();
        }
        Dir = CalcDodgeDirection(Char, MoveInput);
    }
    
    UAnimMontage* UseMontage = GetMontage(Dir);

    if (!UseMontage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    /* ===== 3) PlayMontageAndWait Task 사용 ===== */
    UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        NAME_None,
        UseMontage,
        1.0f,
        NAME_None,
        false, 
        1.0f);

    if (Task)
    {
        Task->OnBlendOut.AddDynamic(this, &UGA_Dodge::OnMontageFinished);
        Task->OnCompleted.AddDynamic(this, &UGA_Dodge::OnMontageFinished);
        Task->OnInterrupted.AddDynamic(this, &UGA_Dodge::OnMontageCancelled);
        Task->OnCancelled.AddDynamic(this, &UGA_Dodge::OnMontageCancelled);

        Task->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    }
}

void UGA_Dodge::OnMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGA_Dodge::OnMontageCancelled()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}




void UGA_Dodge::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    // Dodge 어떤 이유로 끝나든 풀바디 플래그 OFF
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get()))
        {
            NonChar->SetForceFullBody(false);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

/* ========= 방향 계산 ========= */

EDodgeDirection UGA_Dodge::CalcDodgeDirection(const ACharacter* Char, const FVector& WorldInput) const
{
    if (!Char) return EDodgeDirection::Forward;

    FRotator ControlRot = Char->GetControlRotation();
    ControlRot.Pitch = 0.f; 
    ControlRot.Roll = 0.f;

    FVector Forward = ControlRot.Vector();
    FVector Right = FRotationMatrix(ControlRot).GetScaledAxis(EAxis::Y);

    float FwdDot = FVector::DotProduct(WorldInput, Forward);
    float RightDot = FVector::DotProduct(WorldInput, Right);

    // 간단 4방향/8방향 판정 로직 (생략 가능하거나 유지)
    // 여기서는 코드 유지
    bool bFwd = (FwdDot > 0.5f);
    bool bBwd = (FwdDot < -0.5f);
    bool bRight = (RightDot > 0.5f);
    bool bLeft = (RightDot < -0.5f);

    if (bFwd && bRight) return EDodgeDirection::ForwardRight;
    if (bFwd && bLeft) return EDodgeDirection::ForwardLeft;
    if (bBwd && bRight) return EDodgeDirection::BackwardRight;
    if (bBwd && bLeft) return EDodgeDirection::BackwardLeft;
    if (bFwd) return EDodgeDirection::Forward;
    if (bBwd) return EDodgeDirection::Backward;
    if (bRight) return EDodgeDirection::Right;
    if (bLeft) return EDodgeDirection::Left;

    return EDodgeDirection::Forward;
}

/* ========= 방향별 몽타주 선택 ========= */

UAnimMontage* UGA_Dodge::GetMontage(EDodgeDirection Dir) const
{
    switch (Dir)
    {
    case EDodgeDirection::Forward:       return ForwardMontage;
    case EDodgeDirection::Backward:      return BackwardMontage;
    case EDodgeDirection::Left:          return LeftMontage;
    case EDodgeDirection::Right:         return RightMontage;
    case EDodgeDirection::ForwardLeft:   return ForwardLeftMontage;
    case EDodgeDirection::ForwardRight:  return ForwardRightMontage;
    case EDodgeDirection::BackwardLeft:  return BackwardLeftMontage;
    case EDodgeDirection::BackwardRight: return BackwardRightMontage;
    default:                             return ForwardMontage;
    }
}
