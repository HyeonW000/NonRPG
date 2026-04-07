#include "Ability/GA_Revive.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/NonCharacterBase.h"

UGA_Revive::UGA_Revive()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGA_Revive::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get());
    if (!NonChar)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 제자리 부활인지, 근처 부활인지 선택
    UAnimMontage* MontageToPlay = nullptr;
    if (TriggerEventData && TriggerEventData->EventTag == FGameplayTag::RequestGameplayTag(TEXT("Effect.Revive.InPlace")))
    {
        MontageToPlay = ReviveInPlaceMontage;
    }
    else
    {
        MontageToPlay = ReviveNearbyMontage;
    }

    if (MontageToPlay)
    {
        NonChar->SetForceFullBody(true);

        UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this,
            NAME_None,
            MontageToPlay,
            1.0f,
            NAME_None,
            false,
            1.0f
        );

        if (Task)
        {
            Task->OnBlendOut.AddDynamic(this, &UGA_Revive::OnMontageEnded);
            Task->OnCompleted.AddDynamic(this, &UGA_Revive::OnMontageEnded);
            Task->OnInterrupted.AddDynamic(this, &UGA_Revive::OnMontageEnded);
            Task->OnCancelled.AddDynamic(this, &UGA_Revive::OnMontageEnded);
            Task->ReadyForActivation();
        }
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGA_Revive::OnMontageEnded()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Revive::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // [New] 몽타주 종료 후(부활 연출 끝난 후) 3초 동안 무적 유지 후 해제
    if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
        if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Invincible"))))
        {
            FTimerHandle TimerHandle;
            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().SetTimer(TimerHandle, [ASC]()
                {
                    if (ASC)
                    {
                        ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Invincible")));
                    }
                }, 3.0f, false);
            }
        }
    }

    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get()))
    {
        NonChar->SetForceFullBody(false);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
