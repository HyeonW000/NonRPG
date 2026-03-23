#include "Ability/GA_HitReaction.h"
#include "Character/NonCharacterBase.h"
#include "Character/EnemyCharacter.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "BrainComponent.h"

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

    ACharacter* Avatar = Cast<ACharacter>(ActorInfo->AvatarActor);
    if (!Avatar)
    {

        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // AI 컨트롤러 정지 (기절 시 이동/회전 등 모든 뇌 활동 정지)
    if (AAIController* AIC = Cast<AAIController>(Avatar->GetController()))
    {
        AIC->ClearFocus(EAIFocusPriority::Gameplay);
        if (UBrainComponent* Brain = AIC->GetBrainComponent())
        {
            Brain->StopLogic("HitReaction");
        }
    }

    // 1. 트리거 태그 확인
    FGameplayTag HitTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit.Light")); // Default
    if (TriggerEventData && TriggerEventData->EventTag.IsValid())
    {
        HitTag = TriggerEventData->EventTag;
    }

    UE_LOG(LogTemp, Warning, TEXT("[HitReact Debug] GA_HitReaction Activated on %s! Trigger Tag received: %s"), 
        *Avatar->GetName(), *HitTag.ToString());

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

    if (!MontageToPlay)
    {
        UE_LOG(LogTemp, Error, TEXT("[HitReact Debug] FAILED! Could not find any AnimMontage mapped to tag %s in HitMontages Map!"), *HitTag.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[HitReact Debug] SUCCESS! Found AnimMontage: %s mapped to tag %s. Playing now..."), *MontageToPlay->GetName(), *HitTag.ToString());
    }

    // 3. 넉백 처리 (태그에 'Knockback'이나 'Launch'가 포함되어 있다면)
    // 주의: RequestGameplayTag는 태그가 없으면 에러를 낼 수 있음.
    FGameplayTag HitKnockback = UGameplayTagsManager::Get().FindGameplayTagFromPartialString_Slow(TEXT("Effect.Hit.Knockback"));
    FGameplayTag HitKnockdown = UGameplayTagsManager::Get().FindGameplayTagFromPartialString_Slow(TEXT("Effect.Hit.Knockdown"));

    if ((HitKnockback.IsValid() && HitTag.MatchesTag(HitKnockback)) || 
        (HitKnockdown.IsValid() && HitTag.MatchesTag(HitKnockdown)))
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

void UGA_HitReaction::EndAbility(const FGameplayAbilitySpecHandle Handle, 
                                 const FGameplayAbilityActorInfo* ActorInfo, 
                                 const FGameplayAbilityActivationInfo ActivationInfo, 
                                 bool bReplicateEndAbility, bool bWasCancelled)
{
    // AI 컨트롤러 재가동 (기절 끝났을 때 뇌 다시 켬 + 반격 딜레이)
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get()))
        {
            // 스턴(피격) 깨어나자마자 바로 뺨 때리지 않도록 쿨타임(숨고르기) 강제 부여
            if (PostReactionAttackDelay > 0.f)
            {
                Enemy->BlockAttackFor(PostReactionAttackDelay);
            }

            if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
            {
                if (UBrainComponent* Brain = AIC->GetBrainComponent())
                {
                    // 적이 치명상으로 죽은 상태(State.Dead)라면 절대로 뇌를 켜지 않도록 방어 코드 추가
                    if (!Enemy->GetAbilitySystemComponent()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead"))))
                    {
                        Brain->RestartLogic();
                    }
                }
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UGA_HitReaction::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    // 디버그용: 부모 검사 결과 확인
    bool bResult = Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);

    if (!bResult)
    {

    }
    else
    {

    }

    return bResult;
}

bool UGA_HitReaction::ShouldActivateAbility(ENetRole Role) const
{
    bool bResult = Super::ShouldActivateAbility(Role);
    if (!bResult)
    {

    }
    return bResult;
}
