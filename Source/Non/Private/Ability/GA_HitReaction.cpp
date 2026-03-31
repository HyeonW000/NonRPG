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
    ActualReactionDelay = PostReactionAttackDelay; // 기본값으로 세팅

    if (TriggerEventData)
    {
        if (TriggerEventData->EventTag.IsValid())
        {
            HitTag = TriggerEventData->EventTag;
        }
        // EventMagnitude는 현재 데미지(Damage Amount)를 전달하는 데 쓰이고 있으므로, 시간으로 쓰면 안 됨!
    }

    UE_LOG(LogTemp, Warning, TEXT("[HitReact Debug] GA_HitReaction Activated on %s! Trigger Tag received: %s"), 
        *Avatar->GetName(), *HitTag.ToString());

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

    if (!MontageToPlay)
    {
        UE_LOG(LogTemp, Error, TEXT("[HitReact Debug] FAILED! Could not find any AnimMontage mapped to tag %s in HitMontages Map!"), *HitTag.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[HitReact Debug] SUCCESS! Found AnimMontage: %s mapped to tag %s. Playing now..."), *MontageToPlay->GetName(), *HitTag.ToString());
    }

    // 3. 물리 넉백(Launch) 처리 (태그별 차등 넉백)
    if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarActor))
    {
        AActor* Instigator = const_cast<AActor*>(TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr);
        
        // 때린 놈 반대 방향 구하기
        FVector Dir = -AvatarChar->GetActorForwardVector();
        if (Instigator)
        {
            Dir = (AvatarChar->GetActorLocation() - Instigator->GetActorLocation()).GetSafeNormal();
            Dir.Z = 0.f;
        }

        // 맵에서 맞는 태그의 넉백 파워 가져오기
        FKnockbackForce ForceToApply = DefaultKnockback; // 기본값
        if (const FKnockbackForce* FoundForce = TaggedKnockback.Find(HitTag))
        {
            ForceToApply = *FoundForce;
        }

        if (ForceToApply.Strength > 0.f || ForceToApply.Upward > 0.f)
        {
            FVector Impulse = Dir * ForceToApply.Strength;
            Impulse.Z = ForceToApply.Upward;
            AvatarChar->LaunchCharacter(Impulse, true, true);
        }
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
            true // [Fix] 어빌리티가 끝나면 무조건 몽타주를 찢어버림!
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
        // 몽타주 없으면 즉시 종료
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGA_HitReaction::OnMontageEnded()
{
    // 어빌리티 종료
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_HitReaction::OnStunTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    // 스턴 태그(GE_Stun)의 지속 시간이 다 끝나서 몸에서 떨어져 나갔을 때!
    if (NewCount == 0)
    {
        // 몽타주가 얼마나 길건 즉시 끝내버림
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UGA_HitReaction::EndAbility(const FGameplayAbilitySpecHandle Handle, 
                                 const FGameplayAbilityActorInfo* ActorInfo, 
                                 const FGameplayAbilityActivationInfo ActivationInfo, 
                                 bool bReplicateEndAbility, bool bWasCancelled)
{
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
