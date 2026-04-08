#include "Ability/GA_Revive.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

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

    ACharacter* Char = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Char)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1.0f 면 제자리 부활, 그 외(0.0f)면 근처 부활
    const bool bInPlace = (TriggerEventData && TriggerEventData->EventMagnitude > 0.5f);
    UAnimMontage* SelectedMontage = bInPlace ? InPlaceReviveMontage : NearReviveMontage;

    if (SelectedMontage)
    {
        UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this,
            NAME_None,
            SelectedMontage,
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

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
