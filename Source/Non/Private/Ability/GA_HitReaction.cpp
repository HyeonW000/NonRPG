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
    bRetriggerInstancedAbility = true; // [Fix] 이미 피격 중일 때 다시 맞으면 즉시 피격 초기화(무한 경직 허용)

    // 💥 [Fix] 넉다운(State.KnockDown) 중에는 어빌리티 재시전 자체를 100% 원천 차단!
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.KnockDown"), false));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Knockdown"), false));

    // Trigger on Tag
    FAbilityTriggerData Trigger;
    Trigger.TriggerTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit"));
    Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent; 
    AbilityTriggers.Add(Trigger);
}

void UGA_HitReaction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, 
                                      const FGameplayAbilityActorInfo* ActorInfo, 
                                      const FGameplayAbilityActivationInfo ActivationInfo, 
                                      const FGameplayEventData* TriggerEventData)
{
    // 💥 [Fix] ActivateAbility 진입 1등 최우선으로 넉다운 태그(KnockDown/Knockdown) 수신 시 State.KnockDown 즉시 부여!
    if (TriggerEventData && TriggerEventData->EventTag.IsValid())
    {
        FString EvtStr = TriggerEventData->EventTag.ToString();
        if (EvtStr.Contains(TEXT("Knockdown"), ESearchCase::IgnoreCase) || EvtStr.Contains(TEXT("Knockback"), ESearchCase::IgnoreCase))
        {
            if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
            {
                ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.KnockDown"), false));
            }
        }
    }

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

    // 🛡️ [Fix] 전방 가드 성공(bLastGuardSuccess == true)일 때만 일반 피격 어빌리티(GA_HitReaction) 실행 차단!
    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Avatar))
    {
        if (NonChar->IsGuarding() && NonChar->IsLastGuardSuccessful())
        {
            UE_LOG(LogTemp, Warning, TEXT("[HitReact Debug] GA_HitReaction Cancelled because Frontal Guard was Successful on %s!"), *Avatar->GetName());
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
            return;
        }
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
    ActualReactionDelay = PostReactionAttackDelay; // 기본값으로 세팅

    if (TriggerEventData)
    {
        if (TriggerEventData->EventTag.IsValid())
        {
            HitTag = TriggerEventData->EventTag;
        }
    }

    // 💥 [Fix 1] 넉다운(KnockDown) 계열 태그 수신 시 State.KnockDown 상태 태그 부여 및 차단 판정!
    const bool bIsIncomingKnockdown = HitTag.ToString().Contains(TEXT("Knockdown"), ESearchCase::IgnoreCase) || HitTag.ToString().Contains(TEXT("Knockback"), ESearchCase::IgnoreCase);
    
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        static const FGameplayTag KnockdownStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.KnockDown"), false);
        const bool bAlreadyKnockdown = ASC->HasMatchingGameplayTag(KnockdownStateTag) || ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Knockdown"), false));

        // 🛡️ [Fix 2] 이미 넉다운(State.KnockDown) 상태인 경우 일반 약/중 피격에 의해 몽타주가 찢어지는 것을 100% 차단!
        if (bAlreadyKnockdown && !bIsIncomingKnockdown)
        {
            UE_LOG(LogTemp, Warning, TEXT("[GA_HitReaction Debug] BLOCKED incoming non-knockdown hit because character is ALREADY in State.KnockDown!"));
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
            return;
        }

        if (bIsIncomingKnockdown)
        {
            ASC->AddLooseGameplayTag(KnockdownStateTag);
        }
    }

    // 2. 아바타(주인)가 플레이어인지 몬스터인지 확인하여 몽타주를 달라고 요청
    UAnimMontage* MontageToPlay = nullptr;
    AActor* AvatarActor = ActorInfo->AvatarActor.Get();

    if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(AvatarActor))
    {
        MontageToPlay = Player->GetHitMontage(HitTag);
    }
    else if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AvatarActor))
    {
        MontageToPlay = Enemy->GetHitMontage(HitTag);
    }

    UE_LOG(LogTemp, Log, TEXT("💥 [GA_HitReaction] Triggered on '%s' with Tag '%s' -> Found Montage: %s"), 
        *GetNameSafe(AvatarActor), *HitTag.ToString(), *GetNameSafe(MontageToPlay));

    // 3. 몽타주 재생
    if (MontageToPlay)
    {
        UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, 
            NAME_None, 
            MontageToPlay, 
            1.f, 
            NAME_None, 
            true
        );

        Task->OnBlendOut.AddDynamic(this, &UGA_HitReaction::OnMontageEnded);
        Task->OnInterrupted.AddDynamic(this, &UGA_HitReaction::OnMontageEnded);
        Task->OnCancelled.AddDynamic(this, &UGA_HitReaction::OnMontageEnded);
        Task->OnCompleted.AddDynamic(this, &UGA_HitReaction::OnMontageEnded);

        // [New] State.Stunned 태그가 만료되었을 때 어빌리티를 강제로 종료하기 위한 리스너
        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
        {
            FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"));
            TagEventHandle = ASC->RegisterGameplayTagEvent(StunTag, EGameplayTagEventType::NewOrRemoved)
                                .AddUObject(this, &UGA_HitReaction::OnStunTagChanged);
        }

        Task->ReadyForActivation();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ [GA_HitReaction] '%s' 에 피격 몽타주가 등록되어 있지 않습니다! (HitMontages/StanceHitMontages 확인 필요)"), *GetNameSafe(AvatarActor));
        // 몽타주 없으면 즉시 종료
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGA_HitReaction::OnMontageEnded()
{
    // 몽타주 정상 완료 시 State.KnockDown 해제 후 어빌리티 종료
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.KnockDown"), false));
        ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Knockdown"), false));
    }
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_HitReaction::OnStunTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    // 스턴 태그(GE_Stun)의 지속 시간이 다 끝나서 몸에서 떨어져 나갔을 때!
    if (NewCount == 0)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UGA_HitReaction::EndAbility(const FGameplayAbilitySpecHandle Handle, 
                                 const FGameplayAbilityActorInfo* ActorInfo, 
                                 const FGameplayAbilityActivationInfo ActivationInfo, 
                                 bool bReplicateEndAbility, bool bWasCancelled)
{
    // 🛡️ [Fix 3] 강제 취소(bWasCancelled == true)되었을 때는 State.KnockDown 태그를 함부로 지우지 않음!
    if (!bWasCancelled)
    {
        if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
        {
            ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.KnockDown"), false));
            ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Knockdown"), false));
        }
    }

    if (TagEventHandle.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"));
        ActorInfo->AbilitySystemComponent->RegisterGameplayTagEvent(StunTag, EGameplayTagEventType::NewOrRemoved).Remove(TagEventHandle);
        TagEventHandle.Reset();
    }

    // AI 컨트롤러 재가동 (기절 끝났을 때 뇌 다시 켬 + 반격 딜레이)
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get()))
        {
            // 스턴(피격) 깨어나자마자 바로 뺨 때리지 않도록 쿨타임(숨고르기) 강제 부여
            if (ActualReactionDelay > 0.f)
            {
                Enemy->BlockAttackFor(ActualReactionDelay);
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
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get()))
        {
            // 🛡️ 가드 중이더라도 전방 가드가 성공(bLastGuardSuccess == true)했을 때만 피격 어빌리티 차단!
            // (등 뒤 후방 타격으로 가드가 실패했을 때는 일반 피격 어빌리티 정상 발동!)
            if (NonChar->IsGuarding() && NonChar->IsLastGuardSuccessful())
            {
                return false;
            }
        }
    }

    return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool UGA_HitReaction::ShouldActivateAbility(ENetRole Role) const
{
    if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
    {
        if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(CurrentActorInfo->AvatarActor.Get()))
        {
            if (NonChar->IsGuarding() && NonChar->IsLastGuardSuccessful())
            {
                return false;
            }
        }
    }

    return Super::ShouldActivateAbility(Role);
}
