#include "Ability/GA_HitReaction.h"
#include "Character/NonCharacterBase.h"
#include "Character/EnemyCharacter.h"
#include "Character/BossCharacter.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/Engine.h"

UGA_HitReaction::UGA_HitReaction()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bRetriggerInstancedAbility = true; // [Fix] 이미 피격 중일 때 다시 맞으면 즉시 피격 초기화(무한 경직 허용)

    // 💥 [Fix] 넉다운(State.KnockDown) 중에는 어빌리티 재시전 자체를 100% 원천 차단!
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.KnockDown"), false));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Knockdown"), false));

    // ★ 피격 어빌리티 실행 중 캐릭터에게 부여할 상태 태그 (회피, 이동 등 각종 액션 차단용)
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.HitReacting"), false));

    // 피격 시 회피(Dodge) 및 가드(Guard) 등 기존 액션 어빌리티 취소
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Dodge"), false));
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Guard"), false));

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

    // 대상이 플레이어인지 확인 (몬스터와 구분하여 플레이어 대상 디버그 로그 출력)
    bTargetIsPlayer = Avatar->IsPlayerControlled() || (!Avatar->IsA<AEnemyCharacter>() && Avatar->IsA<ANonCharacterBase>());

    // 🛡️ [Fix] 전방 가드 성공(bLastGuardSuccess == true)일 때만 일반 피격 어빌리티(GA_HitReaction) 실행 차단!
    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Avatar))
    {
        if (NonChar->IsGuarding() && NonChar->IsLastGuardSuccessful())
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
            return;
        }
        else if (NonChar->IsGuarding())
        {
            // 가드 중이었으나 방어에 실패하여(후방 피격 등) 맞은 경우, 가드 즉시 강제 해제
            NonChar->StopGuard(true);
        }
    }

    // ★ [핵심 수정: 후퇴/이동 중 피격 시 슬라이딩 완벽 제거]
    // 1) 진행 중이던 후퇴(Flee) 상태 즉시 해제
    if (AEnemyCharacter* EnemyAvatar = Cast<AEnemyCharacter>(Avatar))
    {
        EnemyAvatar->FinishFlee();
    }

    // 2) AI 컨트롤러의 내비게이션 이동(PathFollowing) 및 뇌(BT) 즉시 정지!
    // (StopMovement가 없으면 Brain->StopLogic 후에도 PathFollowingComponent가 후퇴 경로로 캐릭터를 계속 밀어 슬라이딩이 발생함)
    if (AAIController* AIC = Cast<AAIController>(Avatar->GetController()))
    {
        AIC->StopMovement();
        AIC->ClearFocus(EAIFocusPriority::Gameplay);
        if (UBrainComponent* Brain = AIC->GetBrainComponent())
        {
            Brain->StopLogic("HitReaction");
        }
    }

    // 3) 기존 걷기/후퇴 잔여 속도 및 가속도를 즉시 0으로 초기화하여 발 미끄러짐 원천 차단
    if (UCharacterMovementComponent* MoveComp = Avatar->GetCharacterMovement())
    {
        MoveComp->StopActiveMovement();
        MoveComp->StopMovementImmediately();
        MoveComp->Velocity = FVector::ZeroVector;
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

    // 대상이 플레이어 캐릭터인 경우 전신 몽타주 재생을 위해 SetForceFullBody 활성화
    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Avatar))
    {
        NonChar->SetForceFullBody(true);
    }

    // 3. 몽타주 재생
    if (MontageToPlay)
    {
        CurrentMontage = MontageToPlay;

        UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, 
            NAME_None, 
            MontageToPlay, 
            1.f, 
            NAME_None, 
            true
        );

        Task->OnInterrupted.AddDynamic(this, &UGA_HitReaction::OnMontageInterrupted);
        Task->OnCancelled.AddDynamic(this, &UGA_HitReaction::OnMontageCancelled);
        Task->OnCompleted.AddDynamic(this, &UGA_HitReaction::OnMontageCompleted);

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
        if (bTargetIsPlayer)
        {
            UE_LOG(LogTemp, Error, TEXT("❌ [플레이어 피격 실패] '%s' 에 피격 몽타주가 등록되어 있지 않습니다! (태그: %s)"),
                *GetNameSafe(AvatarActor), *HitTag.ToString());
        }
        // 몽타주 없으면 즉시 종료
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // ── 4. [Knockback] 공격자 방향 기반 물리 넉백 처리 (LaunchCharacter) ──
    if (bEnableKnockback && Avatar && Avatar->HasAuthority())
    {
        // 1) 넉백 방향 벡터 산출 (공격자 -> 피격자 방향)
        FVector KnockbackDir = FVector::ZeroVector;
        if (TriggerEventData && TriggerEventData->Instigator != nullptr)
        {
            KnockbackDir = Avatar->GetActorLocation() - TriggerEventData->Instigator->GetActorLocation();
        }

        KnockbackDir.Z = 0.f; // 수평 넉백 우선
        if (KnockbackDir.IsNearlyZero())
        {
            // 공격자 정보가 없거나 같은 위치면 피격자의 뒤쪽으로 밀려남
            KnockbackDir = -Avatar->GetActorForwardVector();
            KnockbackDir.Z = 0.f;
        }
        KnockbackDir.Normalize();

        // 2) 피격 태그별 넉백 강도 산출
        float HorizontalSpeed = LightKnockbackSpeed;
        float VerticalSpeed = LightKnockbackVerticalSpeed;

        const FString TagStr = HitTag.ToString();
        if (TagStr.Contains(TEXT("Knockback"), ESearchCase::IgnoreCase))
        {
            // 넉백 (Knockback): 강하게 붕 떠서 멀리 날려보냄 (공간 확보/블로우)
            HorizontalSpeed = KnockbackHorizontalSpeed;
            VerticalSpeed = KnockbackVerticalSpeed;
        }
        else if (TagStr.Contains(TEXT("Knockdown"), ESearchCase::IgnoreCase))
        {
            // 넉다운 (Knockdown): 콤보 연계를 위해 제자리(또는 반 보)에서 바닥에 털썩 쓰러짐
            HorizontalSpeed = KnockdownHorizontalSpeed;
            VerticalSpeed = KnockdownVerticalSpeed;
        }
        else if (TagStr.Contains(TEXT("Heavy"), ESearchCase::IgnoreCase) || TagStr.Contains(TEXT("Medium"), ESearchCase::IgnoreCase))
        {
            HorizontalSpeed = HeavyKnockbackSpeed;
            VerticalSpeed = HeavyKnockbackVerticalSpeed;
        }
        else
        {
            HorizontalSpeed = LightKnockbackSpeed;
            VerticalSpeed = LightKnockbackVerticalSpeed;
        }

        // 3) 보스 몬스터는 슈퍼아머 느낌을 위해 넉백 강도 대폭 감소
        if (Avatar->IsA<ABossCharacter>())
        {
            HorizontalSpeed *= BossKnockbackScale;
            VerticalSpeed *= BossKnockbackScale;
        }

        // 4) 물리 넉백 적용
        const bool bIsAirborneKnockback = TagStr.Contains(TEXT("Knockback"), ESearchCase::IgnoreCase) || (VerticalSpeed > 10.f);

        if (HorizontalSpeed > 0.1f || VerticalSpeed > 0.1f)
        {
            if (bIsAirborneKnockback)
            {
                // 공중으로 붕 뜨는 진짜 넉백/에어본일 때만 LaunchCharacter 사용 (MOVE_Falling 허용)
                const FVector LaunchVelocity = (KnockbackDir * HorizontalSpeed) + FVector(0.f, 0.f, VerticalSpeed);
                Avatar->LaunchCharacter(LaunchVelocity, true, (VerticalSpeed > 0.1f));
            }
            else
            {
                // 일반 지상 피격(Light/Medium/Heavy):
                // LaunchCharacter를 호출하면 언리얼 엔진 기본 로직에 의해 무조건 MOVE_Falling으로 바뀌어
                // AnimBP가 순간 Air(공중) 상태로 진입하면서 지상 피격 몽타주가 씹히는 현상이 발생합니다.
                // 따라서 지면 접지(MOVE_Walking)를 유지하면서 수평 속도(Velocity)만 직접 밀어냅니다!
                if (UCharacterMovementComponent* MoveComp = Avatar->GetCharacterMovement())
                {
                    FVector NewVel = KnockbackDir * HorizontalSpeed;
                    NewVel.Z = 0.f; // 지면 밀착
                    MoveComp->Velocity = NewVel;
                }
            }

            // 5) [Snap Knockback] 스냅 급제동: 스윽 미끄러지지 않고 짧은 시간 뒤 틱! 하고 딱 멈추도록 수평 관성 리셋
            if (bUseSnapKnockback && !bIsAirborneKnockback && KnockbackDuration > 0.01f)
            {
                TWeakObjectPtr<ACharacter> WeakAvatar = Avatar;
                if (UWorld* World = Avatar->GetWorld())
                {
                    FTimerHandle SnapTimerHandle;
                    World->GetTimerManager().SetTimer(SnapTimerHandle, [WeakAvatar]()
                    {
                        if (ACharacter* TargetChar = WeakAvatar.Get())
                        {
                            if (UCharacterMovementComponent* MoveComp = TargetChar->GetCharacterMovement())
                            {
                                MoveComp->StopActiveMovement();
                                FVector Vel = MoveComp->Velocity;
                                Vel.X = 0.f;
                                Vel.Y = 0.f;
                                MoveComp->Velocity = Vel;
                            }
                        }
                    }, KnockbackDuration, false);
                }
            }
        }
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

void UGA_HitReaction::OnMontageInterrupted()
{
    OnMontageEnded();
}

void UGA_HitReaction::OnMontageCancelled()
{
    OnMontageEnded();
}

void UGA_HitReaction::OnMontageCompleted()
{
    OnMontageEnded();
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
            // 스턴(피격) 깨어나자마자 바로 공격이 나가서 후퇴(카이팅)가 씹히지 않도록 최소 0.25초 숨고르기 보장
            const float EffectiveDelay = (Enemy->bFleeWhenTooClose && Enemy->CombatStyle != EEnemyCombatStyle::Melee)
                ? FMath::Max(ActualReactionDelay, 0.25f)
                : ActualReactionDelay;
            if (EffectiveDelay > 0.f)
            {
                Enemy->BlockAttackFor(EffectiveDelay);
            }

            if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
            {
                const bool bIsDead = Enemy->IsDead() || (Enemy->GetAbilitySystemComponent() && Enemy->GetAbilitySystemComponent()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead"))));
                if (!bIsDead)
                {
                    if (UBrainComponent* Brain = AIC->GetBrainComponent())
                    {
                        Brain->RestartLogic();
                    }

                    // ★ 피격 회복 직후, 순수 원거리 몬스터(또는 이미 근접 공격 목표 횟수를 채운 하이브리드)만 즉시 안전 거리까지 후퇴(카이팅)를 시작!
                    // (하이브리드 몬스터는 맞았다고 바로 도망가지 않고 랜덤 1~3회 근접 반격을 수행한 뒤 거리를 벌림)
                    const bool bIsPureRanged = !Enemy->IsBoss() && ((Enemy->CombatStyle == EEnemyCombatStyle::Ranged) || (Enemy->AttackMontages.Num() == 0));
                    if (!Enemy->IsBoss() && !bWasCancelled && Enemy->bFleeWhenTooClose && (bIsPureRanged || Enemy->bPendingHybridFlee))
                    {
                        AActor* TargetActor = nullptr;
                        if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
                        {
                            TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
                        }
                        // 블랙보드에 대상이 없으면 복제되는 추격 대상으로 대체 (첫 플레이어로 대체하면 협동에서 항상 호스트를 노림)
                        if (!TargetActor)
                        {
                            TargetActor = Enemy->GetAggroTarget();
                        }
                        if (TargetActor)
                        {
                            Enemy->TryPerformBackstepOrFlee(TargetActor);
                        }
                    }
                }
            }
        }
    }

    // 플레이어 캐릭터의 FullBody 강제 모드 해제
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get()))
        {
            NonChar->SetForceFullBody(false);
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
