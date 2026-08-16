#include "Ability/GA_Player_Grabbed.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"

UGA_Player_Grabbed::UGA_Player_Grabbed()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_Player_Grabbed::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!GrabbedMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		GrabbedMontage,
		1.0f,
		NAME_None,
		false
	);

	if (Task)
	{
		Task->OnCompleted.AddDynamic(this, &UGA_Player_Grabbed::OnMontageEnded);
		Task->OnBlendOut.AddDynamic(this, &UGA_Player_Grabbed::OnMontageEnded);
		Task->OnInterrupted.AddDynamic(this, &UGA_Player_Grabbed::OnMontageEnded);
		Task->OnCancelled.AddDynamic(this, &UGA_Player_Grabbed::OnMontageEnded);
		Task->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UGA_Player_Grabbed::OnMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
