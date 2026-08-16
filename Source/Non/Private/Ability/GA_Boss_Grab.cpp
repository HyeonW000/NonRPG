#include "Ability/GA_Boss_Grab.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
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

UGA_Boss_Grab::UGA_Boss_Grab()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
    if (AttackTag.IsValid())
    {
        ActivationOwnedTags.AddTag(AttackTag);
    }

    GrabEventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Grabbed"), false);
    GrabbedStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Grabbed"), false);
}

void UGA_Boss_Grab::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    bIsGrabSuccessful = false;
    GrabbedVictim = nullptr;

    // 잡기 진행 동안 보스 오너에게 State.Attacking 태그 C++ 강제 주입!
    if (UAbilitySystemComponent* BossASC = GetAbilitySystemComponentFromActorInfo())
    {
        FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
        if (AttackTag.IsValid())
        {
            BossASC->AddLooseGameplayTag(AttackTag);
        }
    }

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(ActorInfo->AvatarActor.Get());
    UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] ActivateAbility 시작! (AttemptMontage: %s | SuccessMontage: %s)"), 
        AttemptMontage ? *AttemptMontage->GetName() : TEXT("없음!"),
        SuccessMontage ? *SuccessMontage->GetName() : TEXT("없음!"));

    if (!Enemy || !AttemptMontage)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] AttemptMontage 가 없어서 즉시 EndAbility!"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1. 잡기 시도 몽타주 재생
    UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, AttemptMontage, 1.0f, NAME_None, false
    );

    if (Task)
    {
        Task->OnInterrupted.AddDynamic(this, &UGA_Boss_Grab::OnMontageCancelled);
        Task->OnCancelled.AddDynamic(this, &UGA_Boss_Grab::OnMontageCancelled);
        Task->OnCompleted.AddDynamic(this, &UGA_Boss_Grab::OnMontageEnded);
        Task->ReadyForActivation();

        // 잡기 실패 시 AttemptMontage 재생 시간 완료 후 100% EndAbility 보장 타이머!
        float AttemptLength = AttemptMontage->GetPlayLength();
        FTimerHandle AttemptFallbackHandle;
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(AttemptFallbackHandle, [this]()
            {
                if (!bIsGrabSuccessful)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] AttemptMontage 실패 몽타주 길이 완료 -> EndAbility 해제!"));
                    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
                }
            }, FMath::Max(0.5f, AttemptLength + 0.1f), false);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] MontageTask 생성 실패!"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

bool UGA_Boss_Grab::CheckAndExecuteGrab()
{
    UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] CheckAndExecuteGrab() 판정 호출됨!"));

    if (bIsGrabSuccessful) return true;

    AEnemyCharacter* Boss = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
    if (!Boss) return false;

    FVector StartLoc = Boss->GetActorLocation() + (Boss->GetActorForwardVector() * GrabCheckForwardOffset) + FVector(0.f, 0.f, GrabCheckHeightOffset);
    UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] SphereTrace 시작! 위치: %s | 반지름: %.1f | 소켓: %s"), *StartLoc.ToString(), GrabCheckRadius, *HandSocketName.ToString());
    
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
        EDrawDebugTrace::ForDuration, // 화면에 2.0초간 빨간색 구체 구역 시각 표시!
        HitResults,
        true
    );

    if (bHit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] SphereTrace Hit 성공! Hit 개수: %d개"), HitResults.Num());
        for (const FHitResult& Hit : HitResults)
        {
            ACharacter* Victim = Cast<ACharacter>(Hit.GetActor());
            UE_LOG(LogTemp, Warning, TEXT("  Hit 감지된 대상: %s"), Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("Null"));
            if (Victim && Victim != Boss)
            {
                // 구르기/회피 무적 (State.IFrame) 체크! 무적 상태면 잡기 회피!
                if (IAbilitySystemInterface* VictimASI = Cast<IAbilitySystemInterface>(Victim))
                {
                    if (UAbilitySystemComponent* VictimASC = VictimASI->GetAbilitySystemComponent())
                    {
                        FGameplayTag IFrameTag = FGameplayTag::RequestGameplayTag(TEXT("State.IFrame"), false);
                        if (IFrameTag.IsValid() && VictimASC->HasMatchingGameplayTag(IFrameTag))
                        {
                            UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] %s 가 구르기 무적(State.IFrame) 상태이므로 보스 잡기를 시원하게 회피함!"), *Victim->GetName());

                            // 회피 성공 시 Dodge 텍스트 팝업 생성!
                            if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Victim))
                            {
                                NonChar->Multicast_SpawnDodgeText(Victim->GetActorLocation() + FVector(0.f, 0.f, 100.f));
                            }

                            continue;
                        }
                    }
                }

                // 잡기 성공!
                bIsGrabSuccessful = true;
                GrabbedVictim = Victim;
                UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] %s 잡기 성공! 손 소켓(%s) 부착 및 유저 이동 차단!"), *Victim->GetName(), *HandSocketName.ToString());

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

                        FGameplayTag ActualGrabbedTag = GrabbedStateTag.IsValid() ? GrabbedStateTag : FGameplayTag::RequestGameplayTag(TEXT("State.Grabbed"), false);
                        if (ActualGrabbedTag.IsValid())
                        {
                            VictimASC->AddLooseGameplayTag(ActualGrabbedTag);
                        }

                        FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(TEXT("State.Stun"), false);
                        if (StunTag.IsValid())
                        {
                            VictimASC->AddLooseGameplayTag(StunTag);
                        }

                        // 에디터 디테일 창에서 지정한 잡기 이벤트 태그 전송!
                        FGameplayTag ActualEventTag = GrabEventTag.IsValid() ? GrabEventTag : FGameplayTag::RequestGameplayTag(TEXT("Event.Grabbed"), false);
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
                Victim->AttachToComponent(Boss->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocketName);

                // 3. 시도 몽타주를 멈추고 성공 연출 몽타주 재생!
                if (SuccessMontage)
                {
                    Boss->StopAnimMontage(AttemptMontage);
                    UAbilityTask_PlayMontageAndWait* SuccessTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
                        this, NAME_None, SuccessMontage, 1.0f, NAME_None, false
                    );

                    if (SuccessTask)
                    {
                        SuccessTask->OnCompleted.AddDynamic(this, &UGA_Boss_Grab::OnSuccessMontageEnded);
                        SuccessTask->OnInterrupted.AddDynamic(this, &UGA_Boss_Grab::OnMontageCancelled);
                        SuccessTask->OnCancelled.AddDynamic(this, &UGA_Boss_Grab::OnMontageCancelled);
                        SuccessTask->ReadyForActivation();
                    }

                    // 성공 몽타주 길이만큼 시간이 흐른 뒤 100% OnSuccessMontageEnded 가 불리도록 보장하는 C++ 안전 타이머!
                    float PlayLength = SuccessMontage->GetPlayLength();
                    FTimerHandle MontageFallbackHandle;
                    if (UWorld* World = GetWorld())
                    {
                        World->GetTimerManager().SetTimer(MontageFallbackHandle, [this]()
                        {
                            if (bIsGrabSuccessful)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] SuccessMontage 길이 완료 -> OnSuccessMontageEnded 보장 호출!"));
                                OnSuccessMontageEnded();
                            }
                        }, FMath::Max(0.5f, PlayLength + 0.1f), false);
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
        UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] %s 내팽개치기 순간 손 소켓 분리 완료!"), *GrabbedVictim->GetName());

        GrabbedVictim->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

        if (ACharacter* VictimChar = Cast<ACharacter>(GrabbedVictim))
        {
            if (IAbilitySystemInterface* VictimASI = Cast<IAbilitySystemInterface>(VictimChar))
            {
                if (UAbilitySystemComponent* VictimASC = VictimASI->GetAbilitySystemComponent())
                {
                    FGameplayTag ActualGrabbedTag = GrabbedStateTag.IsValid() ? GrabbedStateTag : FGameplayTag::RequestGameplayTag(TEXT("State.Grabbed"), false);
                    if (ActualGrabbedTag.IsValid())
                    {
                        VictimASC->RemoveLooseGameplayTag(ActualGrabbedTag);
                    }

                    FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(TEXT("State.Stun"), false);
                    if (StunTag.IsValid())
                    {
                        VictimASC->RemoveLooseGameplayTag(StunTag);
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

        GrabbedVictim = nullptr;
    }
}

void UGA_Boss_Grab::OnMontageEnded()
{
    UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] AttemptMontage 시도 몽타주 완전 종료 이벤트(OnCompleted) 감지됨! (성공여부: %s)"), bIsGrabSuccessful ? TEXT("참") : TEXT("거짓"));

    if (bIsGrabSuccessful && SuccessMontage)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] 성공 연출 몽타주가 재생 중이므로 대기"));
        return;
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Boss_Grab::OnMontageCancelled()
{
    UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] 경고: 잡기 몽타주가 강제 캔슬됨"));
    if (GrabbedVictim)
    {
        ReleaseGrabbedVictim();
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Boss_Grab::OnSuccessMontageEnded()
{
    UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] SuccessMontage 성공 연출 몽타주 완전 종료 이벤트(OnCompleted) 감지됨!"));
    ReleaseGrabbedVictim();

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Boss_Grab::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UE_LOG(LogTemp, Warning, TEXT("[GA_Boss_Grab] EndAbility 가 최종 호출되어 잡기 어빌리티가 완전 종료됨!"));

    if (AEnemyCharacter* Boss = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo()))
    {
        // BTTask의 GetAnimatingAbility() 체크가 즉시 Null이 되도록 보스 몽타주 완전 강제 정지!
        if (AttemptMontage) Boss->StopAnimMontage(AttemptMontage);
        if (SuccessMontage) Boss->StopAnimMontage(SuccessMontage);
    }

    if (GrabbedVictim)
    {
        GrabbedVictim->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

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
        FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
        if (AttackTag.IsValid())
        {
            BossASC->RemoveLooseGameplayTag(AttackTag);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
