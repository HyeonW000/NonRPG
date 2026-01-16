#include "Ability/GA_EnemyAttack.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AI/EnemyCharacter.h"
#include "Animation/EnemyAnimSet.h"

UGA_EnemyAttack::UGA_EnemyAttack()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 태그 설정
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack")));
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Attacking")));

    // 다른 어빌리티와의 관계 (사망 시 취소 등은 GA_Death에서 CancelAbilitiesWithTag로 처리됨)
    // 공격 중에는 다른 공격 불가?
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Interact"))); 
}

void UGA_EnemyAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get());
    if (!Enemy)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1. 몽타주 선택 (EnemyAnimSet 사용)
    UAnimMontage* MontageToPlay = nullptr;
    if (UEnemyAnimSet* AnimSet = Enemy->GetAnimSet())
    {
        if (AnimSet->AttackMontages.Num() > 0)
        {
            int32 Index = FMath::RandRange(0, AnimSet->AttackMontages.Num() - 1);
            MontageToPlay = AnimSet->AttackMontages[Index];
        }
    }

    if (!MontageToPlay)
    {
        // 몽타주가 없으면 즉시 종료
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 2. 몽타주 재생
    // 속도(Rate)는 필요시 AttributeSet.AttackSpeed 등 반영 가능 (지금은 1.0)
    UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        NAME_None,
        MontageToPlay,
        1.0f,
        NAME_None,
        false // bStopWhenAbilityEnds
    );

    if (Task)
    {
        Task->OnBlendOut.AddDynamic(this, &UGA_EnemyAttack::OnMontageEnded);
        Task->OnInterrupted.AddDynamic(this, &UGA_EnemyAttack::OnMontageCancelled);
        Task->OnCancelled.AddDynamic(this, &UGA_EnemyAttack::OnMontageCancelled);
        Task->OnCompleted.AddDynamic(this, &UGA_EnemyAttack::OnMontageEnded);
        Task->ReadyForActivation();
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGA_EnemyAttack::OnMontageEnded()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_EnemyAttack::OnMontageCancelled()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_EnemyAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
