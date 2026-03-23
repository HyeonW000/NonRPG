#include "Ability/GA_Death.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/EnemyCharacter.h" 
#include "Character/NonCharacterBase.h"

UGA_Death::UGA_Death()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // [Fix] 하드코딩되었던 태그 설정들을 모두 철거했습니다.
    // 블루프린트(Class Defaults)에서 Trigger, OwnedTags, Cancel/BlockTags를 직접 지정해주세요.
}

void UGA_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

    // 1. 캐릭터 물리/AI 정리 함수 호출 (EnemyCharacter 등에서 구현된 Die/HandleDeath)
    //    우리는 여기서 "기본 정리 로직"만 호출하고, 애니메이션은 이 GA가 담당함.
    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Char))
    {
        // StartDeathSequence: (e.g. Disable Collision/AI/UI) - NOT Animation.
        Enemy->StartDeathSequence(); 
    }
    else if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(Char))
    {
        // Player->HandleDeath();
    }

    // 2. 애니메이션 처리
    
    // 이전에 재생 중이던 공격/피격 몽타주를 완전히 정지 (죽으면서 칼을 휘두르는 현상 방지)
    Char->StopAnimMontage();

    if (DeathMontage && !bUseRagdoll)
    {

        UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this,
            NAME_None,
            DeathMontage,
            1.0f,
            NAME_None,
            false,
            1.0f
        );

        if (Task)
        {
            Task->OnBlendOut.AddDynamic(this, &UGA_Death::OnMontageEnded);
            Task->OnCompleted.AddDynamic(this, &UGA_Death::OnMontageEnded);
            Task->OnInterrupted.AddDynamic(this, &UGA_Death::OnMontageEnded);
            Task->OnCancelled.AddDynamic(this, &UGA_Death::OnMontageEnded);
            Task->ReadyForActivation();
        }
        else
        {
             if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Char))
             {
                 Enemy->StartRagdoll();
             }
        }
    }
    else
    {
        // 몽타주 재생에 완전히 실패한 경우, 선 채로 얼어붙지 않게 곧바로 래그돌/대기 처리
        if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Char))
        {
            Enemy->StartRagdoll();
        }
    }
}

void UGA_Death::OnMontageEnded()
{
    // 애니메이션이 끝나도 사망 상태(State.Dead)는 계속 유지되어야 함.
    // 따라서 EndAbility를 호출하지 않음! (부활할 때 외부에서 Cancel 시켜야 함)
    
    // [Fix] 몽타주 재생이 끝나면 기본 대기(Idle) 모션으로 돌아가면서 시체가 벌떡 일어나는 현상 방지.
    // 마지막 애니메이션 프레임(쓰러진 포즈)에서 캐릭터의 뼈대 관절을 영구적으로 얼려버립니다.
    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo()))
    {
        Enemy->FreezeDeathPose();
    }
}

void UGA_Death::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
