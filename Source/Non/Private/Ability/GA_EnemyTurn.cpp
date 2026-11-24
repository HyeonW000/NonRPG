#include "Ability/GA_EnemyTurn.h"
#include "Character/EnemyCharacter.h"
#include "Character/BossCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "MotionWarpingComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

UGA_EnemyTurn::UGA_EnemyTurn()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bRetriggerInstancedAbility = false;


    FGameplayTag TurnTag = FGameplayTag::RequestGameplayTag(TEXT("State.Turn"), false);
    if (TurnTag.IsValid())
    {
        ActivationOwnedTags.AddTag(TurnTag);
    }

    FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
    if (AttackTag.IsValid())
    {
        ActivationBlockedTags.AddTag(AttackTag); // 공격 중일 때는 회전 어빌리티 실행 절대 금지!
    }
}

void UGA_EnemyTurn::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // ★ 주의: 자식 블루프린트(GA_EnemyTurn_Base 등)의 빈 K2_ActivateAbility 노드가 EndAbility를 즉시 호출해버리는 현상을 막기 위해
    // Super::ActivateAbility 대신 C++에서 직접 CommitAbility 및 로직을 수행합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    AEnemyCharacter* Boss = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
    if (!Boss)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AActor* TargetActor = nullptr;
    if (AAIController* AIC = Cast<AAIController>(Boss->GetController()))
    {
        AIC->StopMovement();
        AIC->ClearFocus(EAIFocusPriority::Gameplay);
        if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
        {
            TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
        }
    }

    if (!TargetActor)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 1. 상대 각도 계산
    float Angle = Boss->GetAngleToTarget(TargetActor);

    // 2. 각도 방향 판단 및 몽타주 선택 (양수: 오른쪽 회전, 음수: 왼쪽 회전)
    // 🌟 몬스터/보스 캐릭터 본체에 등록된 TurnLeft/RightMontage 우선 사용, 없으면 GA 기본값 Fallback!
    UAnimMontage* MontageToPlay = (Angle >= 0.0f) ? Boss->TurnRightMontage : Boss->TurnLeftMontage;
    if (!MontageToPlay)
    {
        MontageToPlay = (Angle >= 0.0f) ? TurnRightMontage : TurnLeftMontage;
    }

    // ★ 만약 회전 몽타주가 비어있다면, 무한 루프에 빠지지 않도록 타겟 방향으로 즉시 회전시킨 뒤 종료!
    if (!MontageToPlay)
    {
        FVector DirToTarget = (TargetActor->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
        if (!DirToTarget.IsNearlyZero())
        {
            FRotator TargetRot = DirToTarget.Rotation();
            TargetRot.Pitch = 0.0f;
            TargetRot.Roll = 0.0f;
            Boss->SetActorRotation(TargetRot);
        }
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // State.Turn 태그 확실히 부여 (BTTask_ActivateAbility가 몽타주 끝날 때까지 대기하도록 보장)
    if (UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent())
    {
        const FGameplayTag TurnTag = FGameplayTag::RequestGameplayTag(TEXT("State.Turn"), false);
        if (TurnTag.IsValid() && !ASC->HasMatchingGameplayTag(TurnTag))
        {
            ASC->AddLooseGameplayTag(TurnTag);
        }
    }

    if (UCharacterMovementComponent* MoveComp = Boss->GetCharacterMovement())
    {
        MoveComp->bOrientRotationToMovement = false;
    }

    // 3. Motion Warping 회전값 설정
    // - 115도 이하인 경우: 1회 회전 애니메이션 동안 Motion Warping이 목표 각도까지 매끄럽게 보간하여 잔여 각도 0도로 정렬!
    // - 115도 초과(완전 후방 등)인 경우: 1회차에 ±90도 회전 후, 남은 각도(> 30도)는 2회차 회전 애니메이션이 연속 재생되어 자연스럽게 2번에 나누어 회전!
    const float ClampedAngle = (FMath::Abs(Angle) <= 115.0f) ? Angle : FMath::Clamp(Angle, -90.0f, 90.0f);
    FRotator TargetRotation = Boss->GetActorRotation();
    TargetRotation.Yaw = FRotator::NormalizeAxis(TargetRotation.Yaw + ClampedAngle);
    TargetRotation.Pitch = 0.0f;
    TargetRotation.Roll = 0.0f;

    if (UMotionWarpingComponent* MotionWarpingComp = Boss->FindComponentByClass<UMotionWarpingComponent>())
    {
        MotionWarpingComp->AddOrUpdateWarpTargetFromTransform(WarpTargetName, FTransform(TargetRotation, Boss->GetActorLocation()));
    }

    // 4. [Multiplayer Fix] GAS AbilityTask + 보스 전용 회전 멀티캐스트(모션 워핑 타겟 포함)로 서버와 클라이언트 모두 100% 동일한 각도 회전 보장!
    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        NAME_None,
        MontageToPlay,
        1.0f,
        NAME_None,
        false
    );

    if (MontageTask)
    {
        MontageTask->OnCompleted.AddDynamic(this, &UGA_EnemyTurn::HandleMontageFinished);
        MontageTask->OnInterrupted.AddDynamic(this, &UGA_EnemyTurn::HandleMontageFinished);
        MontageTask->OnCancelled.AddDynamic(this, &UGA_EnemyTurn::HandleMontageFinished);
        // 주의: OnBlendOut에 바인딩하면 애니메이션 후반부 회전이 끝나기 전에 조기 종료되므로 제외!
        MontageTask->ReadyForActivation();

        if (ABossCharacter* BossChar = Cast<ABossCharacter>(Boss))
        {
            BossChar->Multicast_PlayBossTurnMontage(MontageToPlay, TargetRotation, WarpTargetName);
        }
    }
    else
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGA_EnemyTurn::HandleMontageFinished()
{
    // ★ 강제 SetActorRotation 스냅 제거! (오직 루트 모션 + 모션 워핑으로만 회전하고, 만약 90도 회전 후에도 30도 이상 남았다면 BT가 2회차 회전 애니메이션을 연속 실행함)
    if (AEnemyCharacter* Boss = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo()))
    {
        if (AAIController* AIC = Cast<AAIController>(Boss->GetController()))
        {
            if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
            {
                if (AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor"))))
                {
                    const float FinalAbsAngle = FMath::Abs(Boss->GetAngleToTarget(TargetActor));
                    BB->SetValueAsFloat(TEXT("TargetAngle"), FinalAbsAngle);
                    BB->SetValueAsFloat(TEXT("Angle"), FinalAbsAngle);
                }
            }
        }
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_EnemyTurn::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    AEnemyCharacter* Boss = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
    if (Boss)
    {
        if (UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent())
        {
            const FGameplayTag TurnTag = FGameplayTag::RequestGameplayTag(TEXT("State.Turn"), false);
            if (TurnTag.IsValid())
            {
                ASC->RemoveLooseGameplayTag(TurnTag);
            }
        }

        if (UCharacterMovementComponent* MoveComp = Boss->GetCharacterMovement())
        {
            MoveComp->bOrientRotationToMovement = true;
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_EnemyTurn::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted);
}
