#include "Ability/GA_ConsumePotion.h"
#include "Character/NonCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UGA_ConsumePotion::UGA_ConsumePotion()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Potion")));
    SetAssetTags(AssetTags);

    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Dodge")));
}

void UGA_ConsumePotion::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ACharacter* AvatarChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!AvatarChar)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1. 포션 음용 몽타주 재생
    if (PotionDrinkMontage && AvatarChar->GetMesh())
    {
        if (UAnimInstance* AnimInst = AvatarChar->GetMesh()->GetAnimInstance())
        {
            AnimInst->Montage_Play(PotionDrinkMontage);
        }
    }

    // 2. 0.05초 주기 이동 입력 감시 타이머 시작
    GetWorld()->GetTimerManager().SetTimer(MovementCheckTimerHandle, this, &UGA_ConsumePotion::CheckMovement, 0.05f, true);

    // 3. 주기적 도트 회복 타이머 시작
    GetWorld()->GetTimerManager().SetTimer(HealTimerHandle, this, &UGA_ConsumePotion::ApplyHealTick, PotionTickInterval, true);

    // 4. 복용 완수 타이머 시작
    FTimerHandle EndTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(EndTimerHandle, [this, Handle, ActorInfo, ActivationInfo]() {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }, PotionDuration, false);
}

void UGA_ConsumePotion::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        ACharacter* AvatarChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
        if (AvatarChar && PotionDrinkMontage)
        {
            if (UAnimInstance* AnimInst = AvatarChar->GetMesh()->GetAnimInstance())
            {
                if (AnimInst->Montage_IsActive(PotionDrinkMontage))
                {
                    AnimInst->Montage_Stop(0.2f, PotionDrinkMontage);
                }
            }
        }
    }

    GetWorld()->GetTimerManager().ClearTimer(MovementCheckTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(HealTimerHandle);

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UGameplayEffect* UGA_ConsumePotion::GetCooldownGameplayEffect() const
{
    if (CooldownEffectClass)
    {
        return CooldownEffectClass->GetDefaultObject<UGameplayEffect>();
    }
    return Super::GetCooldownGameplayEffect();
}

void UGA_ConsumePotion::CheckMovement()
{
    ACharacter* AvatarChar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!AvatarChar) return;

    if (AvatarChar->GetVelocity().SizeSquared2D() > 0.01f || !AvatarChar->GetLastMovementInputVector().IsNearlyZero())
    {
        CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
    }
}

void UGA_ConsumePotion::ApplyHealTick()
{
    ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(GetAvatarActorFromActorInfo());
    if (NonChar && NonChar->GetAbilitySystemComponent() && NonChar->GetAttributeSet())
    {
        UAbilitySystemComponent* ASC = NonChar->GetAbilitySystemComponent();
        const UNonAttributeSet* AttrSet = NonChar->GetAttributeSet();

        float MaxHP = AttrSet->GetMaxHP();
        float CurrentHP = AttrSet->GetHP();
        
        float HealAmt = MaxHP * PotionHealPercentPerTick;
        float NewHP = FMath::Min(MaxHP, CurrentHP + HealAmt);

        ASC->SetNumericAttributeBase(AttrSet->GetHPAttribute(), NewHP);
    }
}
