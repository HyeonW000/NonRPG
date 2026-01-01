#include "Ability/GA_Dodge.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "Character/NonCharacterBase.h"

UGA_Dodge::UGA_Dodge()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 여기서 태그 설정 원하면 추가
    // AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Dodge")));
}

void UGA_Dodge::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return; // SP 부족이면 여기서 바로 끝
    }

    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
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

    // FullBody
    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Char))
    {
        NonChar->SetForceFullBody(true);
    }

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    USkeletalMeshComponent* MeshComp = Char->GetMesh();
    if (!MeshComp)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UAnimInstance* Anim = MeshComp->GetAnimInstance();
    if (!Anim)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    /* ===== 2) 방향 계산 ===== */

    /* ===== 2) 방향 계산 (방향 판정 로직만 유지) ===== */
    FVector MoveInput = Char->GetLastMovementInputVector();
    if (MoveInput.IsNearlyZero())
    {
        MoveInput = Char->GetActorForwardVector();
    }
    // 방향 계산 함수 호출
    EDodgeDirection Dir = CalcDodgeDirection(Char, MoveInput);
    UAnimMontage* UseMontage = GetMontage(Dir);

    if (!UseMontage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 이미 같은 몽타주 재생 중이면 무시
    if (Anim->Montage_IsPlaying(UseMontage))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    /* ===== 3) 몽타주 재생 ===== */
    Anim->Montage_Play(UseMontage, 1.0f);

    // 몽타주 끝날 때 EndAbility 호출하도록 델리게이트 바인딩
    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &UGA_Dodge::OnDodgeMontageEnded);
    Anim->Montage_SetEndDelegate(EndDelegate, UseMontage);
}

void UGA_Dodge::OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    // 여기서 Ability 종료
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bInterrupted, false);
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
