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

    ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Char);
    if (NonChar)
    {
        NonChar->KickStaminaRegenDelay();
    }

    /* ===== 2) 방향 계산 ===== */

    FVector MoveInput = Char->GetLastMovementInputVector();
    if (MoveInput.IsNearlyZero())
    {
        MoveInput = Char->GetActorForwardVector();
    }

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
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

/* ========= 방향 계산 ========= */

EDodgeDirection UGA_Dodge::CalcDodgeDirection(const ACharacter* Char, const FVector& WorldInput) const
{
    if (!Char)
    {
        return EDodgeDirection::Forward;
    }

    FVector Input = WorldInput;
    Input.Z = 0.f;

    if (Input.IsNearlyZero())
    {
        return EDodgeDirection::Forward;
    }

    Input.Normalize();

    const FRotator YawRot(0.f, Char->GetActorRotation().Yaw, 0.f);
    const FVector Local = YawRot.UnrotateVector(Input); // Forward:+X, Right:+Y

    const float AngleRad = FMath::Atan2(Local.Y, Local.X);
    float AngleDeg = FMath::RadiansToDegrees(AngleRad);

    auto InRange = [](float Value, float Min, float Max)
        {
            return (Value >= Min && Value < Max);
        };

    if (InRange(AngleDeg, -22.5f, 22.5f))
        return EDodgeDirection::Forward;

    if (InRange(AngleDeg, 22.5f, 67.5f))
        return EDodgeDirection::ForwardRight;

    if (InRange(AngleDeg, 67.5f, 112.5f))
        return EDodgeDirection::Right;

    if (InRange(AngleDeg, 112.5f, 157.5f))
        return EDodgeDirection::BackwardRight;

    if (AngleDeg >= 157.5f || AngleDeg < -157.5f)
        return EDodgeDirection::Backward;

    if (InRange(AngleDeg, -157.5f, -112.5f))
        return EDodgeDirection::BackwardLeft;

    if (InRange(AngleDeg, -112.5f, -67.5f))
        return EDodgeDirection::Left;

    return EDodgeDirection::ForwardLeft;
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
