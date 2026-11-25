#include "Ability/GA_Boss_Grab.h"
#include "Core/NonGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/BossCharacter.h"
#include "Character/EnemyCharacter.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Character/NonCharacterBase.h"
#include "AbilitySystemBlueprintLibrary.h"

UGA_Boss_Grab::UGA_Boss_Grab()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    FGameplayTag AttackTag = NonTags::State_Attacking.GetTag();
    if (AttackTag.IsValid())
    {
        ActivationOwnedTags.AddTag(AttackTag);
    }

    GrabEventTag = NonTags::Event_Grabbed.GetTag();
    GrabbedStateTag = NonTags::State_Grabbed.GetTag();
    ReleaseHitEventTag = NonTags::Effect_Hit_KnockDown.GetTag();
}

void UGA_Boss_Grab::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    bIsGrabSuccessful = false;
    GrabbedVictim = nullptr;

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get());
    UAnimMontage* ActualAttemptMontage = AttemptMontage;
    if (ABossCharacter* BossChar = Cast<ABossCharacter>(Enemy))
    {
        if (BossChar->GrabAttemptMontage)
        {
            ActualAttemptMontage = BossChar->GrabAttemptMontage;
        }
    }

    if (!Enemy || !ActualAttemptMontage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 잡기 진행 동안 보스 오너에게 State.Attacking 태그 C++ 강제 주입!
    if (UAbilitySystemComponent* BossASC = GetAbilitySystemComponentFromActorInfo())
    {
        FGameplayTag AttackTag = NonTags::State_Attacking.GetTag();
        if (AttackTag.IsValid())
        {
            BossASC->AddLooseGameplayTag(AttackTag);
        }
    }

    if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
    {
        AIC->ClearFocus(EAIFocusPriority::Gameplay);
    }
    if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
    {
        MoveComp->bOrientRotationToMovement = false;
    }

    // 1. 잡기 시도 몽타주 재생
    UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, ActualAttemptMontage, 1.0f, NAME_None, false
    );

    if (Task)
    {
        Task->OnInterrupted.AddDynamic(this, &UGA_Boss_Grab::OnMontageCancelled);
        Task->OnCancelled.AddDynamic(this, &UGA_Boss_Grab::OnMontageCancelled);
        Task->OnCompleted.AddDynamic(this, &UGA_Boss_Grab::OnMontageEnded);
        Task->ReadyForActivation();

        // 잡기 실패 시 ActualAttemptMontage 재생 시간 완료 후 100% EndAbility 보장 타이머!
        float AttemptLength = ActualAttemptMontage->GetPlayLength();
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(GrabFallbackTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
            {
                if (IsActive() && !bIsGrabSuccessful)
                {
                    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
                }
            }), FMath::Max(0.5f, AttemptLength + 0.1f), false);
        }
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

bool UGA_Boss_Grab::CheckAndExecuteGrab()
{
    if (bIsGrabSuccessful) return true;

    // 실행 중인 어빌리티 인스턴스에서만 판정 (CDO나 이미 끝난 인스턴스로 불리는 경우 무시)
    if (!IsActive()) return false;

    AEnemyCharacter* Boss = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
    if (!Boss) return false;

    FVector StartLoc = Boss->GetActorLocation() + (Boss->GetActorForwardVector() * GrabCheckForwardOffset) + FVector(0.f, 0.f, GrabCheckHeightOffset);
    
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(Boss);

    TArray<FHitResult> HitResults;
    bool bHit = UKismetSystemLibrary::SphereTraceMulti(
        Boss->GetWorld(),
        StartLoc,
        StartLoc,
        GrabCheckRadius,
        UEngineTypes::ConvertToTraceType(ECC_Pawn),
        false,
        ActorsToIgnore,
        EDrawDebugTrace::None,
        HitResults,
        true
    );

    if (bHit)
    {
        for (const FHitResult& Hit : HitResults)
        {
            ACharacter* Victim = Cast<ACharacter>(Hit.GetActor());
            if (Victim && Victim != Boss)
            {
                // 구르기/회피 무적 (State.IFrame) 체크! 무적 상태면 잡기 회피!
                if (IAbilitySystemInterface* VictimASI = Cast<IAbilitySystemInterface>(Victim))
                {
                    if (UAbilitySystemComponent* VictimASC = VictimASI->GetAbilitySystemComponent())
                    {
                        FGameplayTag IFrameTag = NonTags::State_IFrame.GetTag();
                        if (IFrameTag.IsValid() && VictimASC->HasMatchingGameplayTag(IFrameTag))
                        {
                            // 회피 성공 시 Dodge 텍스트 팝업 생성!
                            if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Victim))
                            {
                                NonChar->HandleIFrameDodge(Boss, Victim->GetActorLocation() + FVector(0.f, 0.f, 100.f)); // 저스트 회피 판정 포함
                            }

                            continue;
                        }
                    }
                }

                // 잡기 성공!
                bIsGrabSuccessful = true;
                GrabbedVictim = Victim;

                // 1. 유저 조작 및 이동 즉시 차단 + 바닥 지면 충돌 마찰로 인한 미끄러짐 방지 (NoCollision)
                if (UCapsuleComponent* Capsule = Victim->GetCapsuleComponent())
                {
                    Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
                if (UCharacterMovementComponent* MoveComp = Victim->GetCharacterMovement())
                {
                    MoveComp->StopMovementImmediately();
                    MoveComp->SetMovementMode(MOVE_None); // 중력 및 물리 틱 완전 오프! (위아래 떨림 100% 사멸)
                }
                if (APlayerController* PC = Cast<APlayerController>(Victim->GetController()))
                {
                    Victim->DisableInput(PC);
                }

                // State.Grabbed 및 State.Stun 태그 C++ 주입 + 현재 유저 공격/스킬 어빌리티 즉시 강제 취소!
                if (IAbilitySystemInterface* VictimASI = Cast<IAbilitySystemInterface>(Victim))
                {
                    if (UAbilitySystemComponent* VictimASC = VictimASI->GetAbilitySystemComponent())
                    {
                        // 잡히는 그 즉시 유저가 휘두르던 칼/스킬 모션 100% 강제 취소!
                        VictimASC->CancelAllAbilities();

                        // 잡힘 태그는 클라이언트까지 복제 (본인 화면의 입력 차단과 UI가 이 태그를 봄)
                        FGameplayTag ActualGrabbedTag = GrabbedStateTag.IsValid() ? GrabbedStateTag : NonTags::State_Grabbed.GetTag();
                        if (ActualGrabbedTag.IsValid())
                        {
                            VictimASC->SetLooseGameplayTagCount(ActualGrabbedTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
                        }

                        // 에디터 디테일 창에서 지정한 잡기 이벤트 태그 전송!
                        FGameplayTag ActualEventTag = GrabEventTag.IsValid() ? GrabEventTag : NonTags::Event_Grabbed.GetTag();
                        if (ActualEventTag.IsValid())
                        {
                            FGameplayEventData Payload;
                            Payload.EventTag = ActualEventTag;
                            Payload.Instigator = GetAvatarActorFromActorInfo();
                            Payload.Target = Victim;
                            VictimASC->HandleGameplayEvent(ActualEventTag, &Payload);
                        }
                    }
                }

                // 2. 유저를 보스 손 소켓 위치로 딱 들어오도록 부착 (SnapToTargetNotIncludingScale 사용)
                FName ActualHandSocket = HandSocketName;
                UAnimMontage* ActualSuccessMontage = SuccessMontage;
                if (ABossCharacter* BossChar = Cast<ABossCharacter>(Boss))
                {
                    if (BossChar->GrabSuccessMontage)
                    {
                        ActualSuccessMontage = BossChar->GrabSuccessMontage;
                    }
                    if (!BossChar->GrabHandSocketName.IsNone())
                    {
                        ActualHandSocket = BossChar->GrabHandSocketName;
                    }
                }

                Victim->AttachToComponent(Boss->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, ActualHandSocket);

                // 3. 시도 몽타주를 멈추고 성공 연출 몽타주 재생!
                if (ActualSuccessMontage)
                {
                    Boss->StopAnimMontage(AttemptMontage);
                    UAbilityTask_PlayMontageAndWait* SuccessTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
                        this, NAME_None, ActualSuccessMontage, 1.0f, NAME_None, false
                    );

                    if (SuccessTask)
                    {
                        SuccessTask->OnCompleted.AddDynamic(this, &UGA_Boss_Grab::OnSuccessMontageEnded);
                        SuccessTask->OnInterrupted.AddDynamic(this, &UGA_Boss_Grab::OnSuccessMontageCancelled);
                        SuccessTask->OnCancelled.AddDynamic(this, &UGA_Boss_Grab::OnSuccessMontageCancelled);
                        SuccessTask->ReadyForActivation();
                    }

                    // 성공 몽타주 길이만큼 시간이 흐른 뒤 100% OnSuccessMontageEnded 가 불리도록 보장하는 C++ 안전 타이머!
                    // 실제 재생한 몽타주 기준 (SuccessMontage는 비어 있고 보스의 GrabSuccessMontage를 쓰는 경우가 있음)
                    const float PlayLength = ActualSuccessMontage->GetPlayLength();
                    // 같은 핸들에 다시 걸면 잡기 시도용 타이머는 자동으로 해제됨
                    if (UWorld* World = GetWorld())
                    {
                        World->GetTimerManager().SetTimer(GrabFallbackTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
                        {
                            if (IsActive() && bIsGrabSuccessful)
                            {
                                OnSuccessMontageEnded();
                            }
                        }), FMath::Max(0.5f, PlayLength + 0.1f), false);
                    }
                }
                else
                {
                    // SuccessMontage가 등록되어 있지 않은 경우 즉시 릴리즈 및 종료 처리하여 멍때림 방지!
                    OnSuccessMontageEnded();
                }

                return true;
            }
        }
    }

    return false;
}

void UGA_Boss_Grab::ReleaseGrabbedVictim()
{
    if (GrabbedVictim)
    {
        GrabbedVictim->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        // 손 소켓 각도대로 기울어진 채 남지 않게 좌우 방향만 남기고 세움 (본인 클라이언트는 OnRep_AttachmentReplication에서 세움)
        {
            FRotator UprightRot = GrabbedVictim->GetActorRotation();
            UprightRot.Pitch = 0.f;
            UprightRot.Roll = 0.f;
            GrabbedVictim->SetActorRotation(UprightRot, ETeleportType::TeleportPhysics);
        }

        if (ACharacter* VictimChar = Cast<ACharacter>(GrabbedVictim))
        {
            if (IAbilitySystemInterface* VictimASI = Cast<IAbilitySystemInterface>(VictimChar))
            {
                if (UAbilitySystemComponent* VictimASC = VictimASI->GetAbilitySystemComponent())
                {
                    FGameplayTag ActualGrabbedTag = GrabbedStateTag.IsValid() ? GrabbedStateTag : NonTags::State_Grabbed.GetTag();
                    if (ActualGrabbedTag.IsValid())
                    {
                        VictimASC->SetLooseGameplayTagCount(ActualGrabbedTag, 0, EGameplayTagReplicationState::TagAndCountToAll);
                    }
                }
            }

            if (UCapsuleComponent* Capsule = VictimChar->GetCapsuleComponent())
            {
                Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
            if (UCharacterMovementComponent* MoveComp = VictimChar->GetCharacterMovement())
            {
                MoveComp->SetMovementMode(MOVE_Walking);
            }
            if (APlayerController* PC = Cast<APlayerController>(VictimChar->GetController()))
            {
                VictimChar->EnableInput(PC);
            }
        }

        // 데미지 적용
        if (GrabDamageEffectClass)
        {
            FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(GrabDamageEffectClass);
            if (SpecHandle.IsValid())
            {
                if (IAbilitySystemInterface* VictimASI = Cast<IAbilitySystemInterface>(GrabbedVictim))
                {
                    if (UAbilitySystemComponent* VictimASC = VictimASI->GetAbilitySystemComponent())
                    {
                        VictimASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
                    }
                }
            }
        }

        // 던져짐: 넉다운 피격 이벤트 -> 피해자의 GA_HitReaction이 넉다운 몽타주 재생 + 보스 반대 방향으로 밀림
        // (잡기 데미지로 사망했으면 사망 처리가 우선이므로 보내지 않음)
        if (ReleaseHitEventTag.IsValid())
        {
            const ANonCharacterBase* VictimNonChar = Cast<ANonCharacterBase>(GrabbedVictim);
            const bool bVictimDead = VictimNonChar && VictimNonChar->IsDead();
            if (!bVictimDead)
            {
                FGameplayEventData Payload;
                Payload.EventTag = ReleaseHitEventTag;
                Payload.Instigator = GetAvatarActorFromActorInfo();
                Payload.Target = GrabbedVictim;
                UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GrabbedVictim, ReleaseHitEventTag, Payload);
            }
        }

        GrabbedVictim = nullptr;
    }
}

void UGA_Boss_Grab::OnMontageEnded()
{
    // 시도 몽타주 종료: 잡기에 성공했으면 성공 몽타주가 이어서 처리하므로 무시
    if (bIsGrabSuccessful)
    {
        return;
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Boss_Grab::OnMontageCancelled()
{
    // 시도 몽타주 중단: 잡기 성공으로 성공 몽타주로 넘어가며 끊긴 것이면 무시 (여기서 놓아 버리면 잡자마자 풀림)
    if (bIsGrabSuccessful)
    {
        return;
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Boss_Grab::OnSuccessMontageCancelled()
{
    // 성공 몽타주 중단 (보스 피격/사망 등): 잡은 대상을 놓고 종료
    if (GrabbedVictim)
    {
        ReleaseGrabbedVictim();
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Boss_Grab::OnSuccessMontageEnded()
{
    ReleaseGrabbedVictim();

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Boss_Grab::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 끝난 뒤에 남은 종료 보장 타이머가 다음 발동을 끝내지 않게 해제
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(GrabFallbackTimerHandle);
    }

    if (AEnemyCharacter* Boss = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo()))
    {
        // BTTask의 GetAnimatingAbility() 체크가 즉시 Null이 되도록 보스 몽타주 완전 강제 정지!
        if (AttemptMontage) Boss->StopAnimMontage(AttemptMontage);
        if (SuccessMontage) Boss->StopAnimMontage(SuccessMontage);
    }

    if (GrabbedVictim)
    {
        GrabbedVictim->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        {
            FRotator UprightRot = GrabbedVictim->GetActorRotation();
            UprightRot.Pitch = 0.f;
            UprightRot.Roll = 0.f;
            GrabbedVictim->SetActorRotation(UprightRot, ETeleportType::TeleportPhysics);
        }

        // 잡힘 태그 해제 (보스 사망 등으로 성공 몽타주 없이 끝나는 경우에도 입력이 풀리도록)
        if (IAbilitySystemInterface* VictimASI = Cast<IAbilitySystemInterface>(GrabbedVictim))
        {
            if (UAbilitySystemComponent* VictimASC = VictimASI->GetAbilitySystemComponent())
            {
                const FGameplayTag ActualGrabbedTag = GrabbedStateTag.IsValid() ? GrabbedStateTag : NonTags::State_Grabbed.GetTag();
                VictimASC->SetLooseGameplayTagCount(ActualGrabbedTag, 0, EGameplayTagReplicationState::TagAndCountToAll);
            }
        }

        if (ACharacter* VictimChar = Cast<ACharacter>(GrabbedVictim))
        {
            if (UCapsuleComponent* Capsule = VictimChar->GetCapsuleComponent())
            {
                Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
            if (UCharacterMovementComponent* MoveComp = VictimChar->GetCharacterMovement())
            {
                MoveComp->SetMovementMode(MOVE_Walking);
            }
            if (APlayerController* PC = Cast<APlayerController>(VictimChar->GetController()))
            {
                VictimChar->EnableInput(PC);
            }
        }

        GrabbedVictim = nullptr;
    }

    bIsGrabSuccessful = false;

    // 어빌리티 완전 종료 시 State.Attacking 태그 C++ 강제 제거!
    if (UAbilitySystemComponent* BossASC = GetAbilitySystemComponentFromActorInfo())
    {
        FGameplayTag AttackTag = NonTags::State_Attacking.GetTag();
        if (AttackTag.IsValid())
        {
            BossASC->RemoveLooseGameplayTag(AttackTag);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
