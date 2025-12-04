#include "Ability/GA_ToggleWeapon.h"

#include "Character/NonCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UGA_ToggleWeapon::UGA_ToggleWeapon()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 태그는 BP에서 세팅해도 되고, 여기서 기본값을 줄 수도 있음
    // 예)
    // AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.ToggleWeapon")));
}

void UGA_ToggleWeapon::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Character);
    if (!NonChar)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    CachedNonChar = NonChar;

    // ── 1) 이번 토글에서의 "목표 무장 상태" 계산 ──
    // 현재 무장 상태의 반대 = 이번 토글의 목표 상태
    bTargetArmed = !NonChar->IsArmed();

    UAnimMontage* SelectedMontage = nullptr;

    if (bTargetArmed)
    {
        // 무장 쪽 몽타주 하나만 사용 (원하는 쪽 골라)
        SelectedMontage = DrawMontage;   // 또는 DrawUpperBodyMontage
    }
    else
    {
        // 비무장 쪽 몽타주 하나만 사용
        SelectedMontage = SheatheMontage; // 또는 SheatheUpperBodyMontage
    }

    // ── 3) 몽타주 재생 ──
    UAnimInstance* AnimInstance = Character->GetMesh()
        ? Character->GetMesh()->GetAnimInstance()
        : nullptr;

    if (!AnimInstance)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ActiveMontage = SelectedMontage;

    AnimInstance->Montage_Play(SelectedMontage, 1.0f);

    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &UGA_ToggleWeapon::OnToggleMontageEnded);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, SelectedMontage);
}

void UGA_ToggleWeapon::OnToggleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage != ActiveMontage)
    {
        return;
    }

    // 이제 무장 상태는 AnimNotify_SwapWeaponSocket에서 이미 처리함
    ActiveMontage = nullptr;
    CachedNonChar = nullptr;

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bInterrupted, false);
}


