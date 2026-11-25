#include "Ability/GA_ConsumePotion.h"
#include "Core/NonGameplayTags.h"
#include "Character/NonCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UGA_ConsumePotion::UGA_ConsumePotion()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(NonTags::Ability_Potion.GetTag());
    SetAssetTags(AssetTags);

    CancelAbilitiesWithTag.AddTag(NonTags::Ability_Dodge.GetTag());

    // 서버의 물약 사용 처리(InventoryComponent::ServerUseConsumable)가 Ability.Potion 이벤트로 실행.
    // 서버에서 시작하고 소유 클라이언트에도 활성화를 전달(몽타주 로컬 재생)
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    {
        FAbilityTriggerData Trigger;
        Trigger.TriggerTag = NonTags::Ability_Potion.GetTag();
        Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
        AbilityTriggers.Add(Trigger);
    }

    // 설계서 11장 확정: 전투 중(State.InCombat / State.Combat)에는 회복 물약 사용 불가
    ActivationBlockedTags.AddTag(NonTags::State_Combat.GetTag());
    ActivationBlockedTags.AddTag(NonTags::State_InCombat.GetTag());
    ActivationBlockedTags.AddTag(NonTags::State_Dead.GetTag());
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

    // 2. 틱당 회복량 계산 (아이템 EffectValue: 1 이하 = 최대 HP 비율, 1 초과 = 고정량)
    HealPerTick = 0.f;
    if (const ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(AvatarChar))
    {
        if (const UNonAttributeSet* AttrSet = NonChar->GetAttributeSet())
        {
            const float ItemValue = TriggerEventData ? TriggerEventData->EventMagnitude : 0.f;
            const float TotalHeal = (ItemValue > 1.f)
                ? ItemValue
                : AttrSet->GetMaxHP() * ((ItemValue > 0.f) ? ItemValue : DefaultTotalHealRatio);
            const int32 NumTicks = FMath::Max(1, FMath::FloorToInt(PotionDuration / FMath::Max(0.05f, PotionTickInterval)));
            HealPerTick = TotalHeal / NumTicks;
        }
    }

    // 3. 0.05초 주기 이동 감시 (이동하면 복용 취소)
    GetWorld()->GetTimerManager().SetTimer(MovementCheckTimerHandle, this, &UGA_ConsumePotion::CheckMovement, 0.05f, true);

    // 4. 주기적 도트 회복 (회복은 서버에서만)
    if (ActorInfo->IsNetAuthority())
    {
        GetWorld()->GetTimerManager().SetTimer(HealTimerHandle, this, &UGA_ConsumePotion::ApplyHealTick, PotionTickInterval, true);
    }

    // 5. 복용 완료 타이머
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
    GetWorld()->GetTimerManager().ClearTimer(EndTimerHandle);

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

        const float MaxHP = AttrSet->GetMaxHP();
        const float OldHP = AttrSet->GetHP();
        const float NewHP = FMath::Min(MaxHP, OldHP + HealPerTick);

        ASC->SetNumericAttributeBase(AttrSet->GetHPAttribute(), NewHP);

        // 플로팅 힐 숫자: 이번 틱에 실제로 찬 양만 표시 (HP가 가득 차서 0이면 표시 안 함)
        const float Healed = NewHP - OldHP;
        if (Healed >= 0.5f)
        {
            NonChar->Multicast_SpawnHealNumber(Healed, NonChar->GetActorLocation());
        }
    }
}
