#include "Ability/GA_Death.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AI/EnemyCharacter.h" 
#include "Character/NonCharacterBase.h"

UGA_Death::UGA_Death()
{
    // 서버가 주도하고 클라이언트에 복제
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // Trigger: Effect.Death 이벤트 수신 시 자동 발동
    FAbilityTriggerData TriggerData;
    TriggerData.TriggerTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Death"));
    TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
    AbilityTriggers.Add(TriggerData);

    // [Tags] 이 어빌리티가 활성화된 동안 State.Dead 태그 유지
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead")));
    
    // [Cancel] 사망 시 모든 행동 취소
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability")));
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State")));
    
    // [Block] 사망 중에는 다른 어떤 것도 못함
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State")));
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
    }
    else
    {
        // 몽타주가 없거나 래그돌 사용 시 -> 즉시 종료 (또는 래그돌 시작)
        // 래그돌 로직은 보통 캐릭터 함수에 위임
        if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Char))
        {
            Enemy->StartRagdoll();
        }
        
        // 지속형이 아니므로 즉시 종료 (State.Dead 태그는 남겨야 하는데...?)
        // [중요] GA가 끝나면 ActivationOwnedTags(State.Dead)가 사라진다.
        // 하지만 죽음은 영구적이어야 한다.
        // 방법 1: GA를 영원히 유지한다 (EndAbility 호출 안함). -> 부활 시 CancelAbility 호출.
        // 방법 2: GA는 연출만 하고, State.Dead 태그 자체는 LooseTag로 캐릭터에 박아버린다.
        
        // 여기서는 "방법 1"을 선택하겠습니다. (부활 전까지 GA 유지)
        // 단, 몽타주 끝나면?
    }
}

void UGA_Death::OnMontageEnded()
{
    // 애니메이션이 끝나도 사망 상태(State.Dead)는 계속 유지되어야 함.
    // 따라서 EndAbility를 호출하지 않음! (부활할 때 외부에서 Cancel 시켜야 함)
}

void UGA_Death::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
