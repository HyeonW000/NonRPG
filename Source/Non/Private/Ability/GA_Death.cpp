#include "Ability/GA_Death.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/EnemyCharacter.h" 
#include "Character/NonCharacterBase.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

UGA_Death::UGA_Death()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 1) 기본 에셋 태그 (Ability.Death)
    FGameplayTag DeathAbilityTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Death"), false);
    if (DeathAbilityTag.IsValid())
    {
        FGameplayTagContainer DefaultAssetTags;
        DefaultAssetTags.AddTag(DeathAbilityTag);
        SetAssetTags(DefaultAssetTags);
    }

    // 2) 발동 시 캐릭터에게 주는 태그 (State.Dead)
    FGameplayTag DeadStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false);
    if (DeadStateTag.IsValid())
    {
        ActivationOwnedTags.AddTag(DeadStateTag);
    }

    // 3) 자동 트리거: 체력이 0이 될 때 발행되는 Effect.Death 게임플레이 이벤트 자동 감지
    FGameplayTag DeathEventTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Death"), false);
    if (DeathEventTag.IsValid())
    {
        FAbilityTriggerData TriggerData;
        TriggerData.TriggerTag = DeathEventTag;
        TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
        AbilityTriggers.Add(TriggerData);
    }

    // 4) 사망 시 즉시 취소할 기존 어빌리티 태그 (공격, 피격 리액션 등 캔슬)
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false));
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.HitReacting"), false));
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack"), false));

    // 5) 사망 중 신규 일반 어빌리티 발동 차단
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability"), false));
}

bool UGA_Death::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    // 이미 State.Dead 태그를 보유하고 있다면 중복 실행 방지
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false);
        if (DeadTag.IsValid() && ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(DeadTag))
        {
            return false;
        }
    }

    // 사망 어빌리티는 공격 중, 경직 중 등 어떠한 상태에서도 즉시 발동되어야 함
    return true;
}

void UGA_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    bDeathPoseFrozen = false;

    ACharacter* Char = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Char)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 🔔 [디버그 로그 및 화면 출력]
    const FString CharName = Char->GetName();
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Emerald, 
            FString::Printf(TEXT("[사망 디버그] GA_Death 발동 성공! 대상: %s"), *CharName));
    }
    UE_LOG(LogTemp, Warning, TEXT("[사망 디버그] GA_Death 발동 성공! 대상: %s"), *CharName);

    // 1. 캐릭터 물리/AI 정리 함수 호출 (이동 정지, 캡슐 Ignore 등 - UnPossess는 제외됨)
    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Char))
    {
        Enemy->StartDeathSequence(); 
    }
    else if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(Char))
    {
        Player->HandleDeath();
    }

    Char->StopAnimMontage();

    // 2. 사망 몽타주 결정
    UAnimMontage* MontageToPlay = DeathMontage;
    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Char))
    {
        if (Enemy->DeathMontage)
        {
            MontageToPlay = Enemy->DeathMontage;
        }
    }
    else if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Char))
    {
        if (UAnimMontage* StanceMontage = NonChar->GetDeathMontage())
        {
            MontageToPlay = StanceMontage;
        }
    }

    // [Fail-safe] 골렘 보스인데 몽타주가 비어있는 경우 골렘 전용 사망 몽타주 자동 로드
    if (!MontageToPlay && CharName.Contains(TEXT("Golem"), ESearchCase::IgnoreCase))
    {
        MontageToPlay = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, TEXT("/Game/Non/Animation/Enemy/Golem/AM_Golem_Death.AM_Golem_Death")));
    }

    const FString MontageName = MontageToPlay ? MontageToPlay->GetName() : TEXT("None (몽타주 없음!)");
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, MontageToPlay ? FColor::Cyan : FColor::Red,
            FString::Printf(TEXT("[사망 디버그] 대상: %s | 사망 몽타주: %s"), *CharName, *MontageName));
    }
    UE_LOG(LogTemp, Warning, TEXT("[사망 디버그] 대상: %s | 사망 몽타주: %s"), *CharName, *MontageName);

    if (MontageToPlay && !bUseRagdoll)
    {
        MontageToPlay->bEnableAutoBlendOut = false;

        const float MontageLength = MontageToPlay->GetPlayLength();
        // 몽타주 전체가 재생되고 마지막 쓰러진 프레임에 도달할 때 고정 (길이 - 0.05초)
        const float SafeFreezeDelay = FMath::Max(0.5f, MontageLength - 0.05f);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
                FString::Printf(TEXT("[사망 디버그] 몽타주 재생 시작! 총 길이: %.2f초 (고정 대기: %.2f초)"), MontageLength, SafeFreezeDelay));
        }
        UE_LOG(LogTemp, Warning, TEXT("[사망 디버그] 몽타주 재생 시작! 총 길이: %.2f초 (고정 대기: %.2f초)"), MontageLength, SafeFreezeDelay);

        // 1) 비동기 어빌리티 태스크 등록 및 단일 실행
        UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this,
            NAME_None,
            MontageToPlay,
            1.0f,
            NAME_None,
            false,
            1.0f
        );

        if (Task)
        {
            // ★ 중요: OnInterrupted, OnCancelled, OnBlendOut에서 조기 프리징하지 않습니다!
            // 오직 애니메이션이 끝까지 재생된 OnCompleted 시점에만 고정합니다.
            Task->OnCompleted.AddDynamic(this, &UGA_Death::OnMontageEnded);
            Task->ReadyForActivation();
        }
        else
        {
            // 태스크 생성이 불가능한 예외 환경에서만 직접 PlayAnimMontage 호출
            Char->PlayAnimMontage(MontageToPlay, 1.0f);
        }

        if (Char->GetMesh())
        {
            if (UAnimInstance* AnimInst = Char->GetMesh()->GetAnimInstance())
            {
                if (FAnimMontageInstance* Inst = AnimInst->GetActiveInstanceForMontage(MontageToPlay))
                {
                    Inst->bEnableAutoBlendOut = false;
                }
            }
        }

        // 2) 세이프티 타이머: 태스크 콜백 누락 여부와 상관없이 몽타주 길이만큼 끝까지 감상한 후 안전하게 고정 보장!
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(DeathTimerHandle);
            World->GetTimerManager().SetTimer(DeathTimerHandle, this, &UGA_Death::OnMontageEnded, SafeFreezeDelay, false);
        }
    }
    else
    {
        // 몽타주가 없거나 bUseRagdoll일 경우 래그돌 처리
        if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Char))
        {
            Enemy->StartRagdoll();
        }
    }
}

void UGA_Death::OnMontageEnded()
{
    if (bDeathPoseFrozen) return;
    bDeathPoseFrozen = true;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DeathTimerHandle);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("[사망 디버그] 사망 몽타주 완전히 종료 -> FreezeDeathPose 고정!"));
    }
    UE_LOG(LogTemp, Warning, TEXT("[사망 디버그] 사망 몽타주 완전히 종료 -> FreezeDeathPose 고정!"));

    // 몽타주 재생이 끝나면 마지막 쓰러진 포즈로 영구 고정
    if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetAvatarActorFromActorInfo()))
    {
        Char->FreezeDeathPose();
    }
    else if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo()))
    {
        Enemy->FreezeDeathPose();
    }
}

void UGA_Death::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
