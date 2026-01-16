#include "Ability/GA_HitReaction.h"
#include "Character/NonCharacterBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/CharacterMovementComponent.h"

UGA_HitReaction::UGA_HitReaction()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // Trigger on Tag
    FAbilityTriggerData Trigger;
    Trigger.TriggerTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit"));
    Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent; 
    // 주의: Effect.Hit 태그가 Event로 오는지, 아니면 OwnedTag로 추가되는지에 따라 다름.
    // 보통은 AttributeSet에서 PostGameplayEffectExecute 때 SendGameplayEvent를 호출해줘야 함.
    // 혹은 TagAdded 트리거를 쓸 수도 있음. 
    // 여기서는 "GameplayEvent" 방식을 가정하고, AttributeSet 수정을 병행합니다.
    AbilityTriggers.Add(Trigger);
}

void UGA_HitReaction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, 
                                      const FGameplayAbilityActorInfo* ActorInfo, 
                                      const FGameplayAbilityActivationInfo ActivationInfo, 
                                      const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ANonCharacterBase* Avatar = Cast<ANonCharacterBase>(ActorInfo->AvatarActor);
    if (!Avatar)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1. 트리거 태그 확인 (이벤트 데이터에 태그가 들어온다고 가정)
    //    만약 EventData가 비어있다면, OwnedTags를 뒤져야 할 수도 있음.
    FGameplayTag HitTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit.Light")); // Default
    if (TriggerEventData && TriggerEventData->EventTag.IsValid())
    {
        HitTag = TriggerEventData->EventTag;
    }

    // 2. 몽타주 선택
    UAnimMontage* MontageToPlay = nullptr;

    // 정확히 매칭되거나, 자식 태그 포함 검색
    // 예: Effect.Hit.Knockdown으로 트리거되면 -> 맵에서 Effect.Hit.Knockdown 찾기
    if (UAnimMontage** Found = HitMontages.Find(HitTag))
    {
        MontageToPlay = *Found;
    }
    else
    {
        // 못 찾으면 부모 태그 검색 or Default (Effect.Hit.Light)
        for (auto& Elem : HitMontages)
        {
            if (HitTag.MatchesTag(Elem.Key))
            {
                MontageToPlay = Elem.Value;
                break;
            }
        }
    }

    // 3. 넉백 처리 (태그에 'Knockback'이나 'Launch'가 포함되어 있다면)
    if (HitTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit.Knockback"))) ||
        HitTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit.Knockdown"))))
    {
        // 가해자 위치 추정 (EventData Context)
        AActor* Instigator = const_cast<AActor*>(TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr);
        
        FVector Dir = -Avatar->GetActorForwardVector(); // Default: Backwards
        if (Instigator)
        {
            Dir = (Avatar->GetActorLocation() - Instigator->GetActorLocation()).GetSafeNormal();
            Dir.Z = 0.f;
        }

        // 넉다운은 좀 더 세게?
        float Strength = DefaultKnockbackStrength;
        float Upward = DefaultKnockbackUpward;
        
        if (HitTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit.Knockdown"))))
        {
            Strength *= 1.5f;
            Upward *= 1.5f;
        }

        FVector Impulse = Dir * Strength;
        Impulse.Z = Upward;

        Avatar->LaunchCharacter(Impulse, true, true);
    }

    // 4. 몽타주 재생
    if (MontageToPlay)
    {
        UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, 
            NAME_None, 
            MontageToPlay, 
            1.f, 
            NAME_None, 
            false // bStopWhenAbilityEnds
        );

        Task->OnBlendOut.AddDynamic(this, &UGA_HitReaction::OnMontageEnded);
        Task->OnInterrupted.AddDynamic(this, &UGA_HitReaction::OnMontageEnded);
        Task->OnCancelled.AddDynamic(this, &UGA_HitReaction::OnMontageEnded);
        Task->OnCompleted.AddDynamic(this, &UGA_HitReaction::OnMontageEnded);

        Task->ReadyForActivation();
    }
    else
    {
        // 몽타주 없으면 즉시 종료
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGA_HitReaction::OnMontageEnded()
{
    // 어빌리티 종료
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
