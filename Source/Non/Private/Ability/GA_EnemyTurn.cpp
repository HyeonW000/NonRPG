#include "Ability/GA_EnemyTurn.h"
#include "Character/EnemyCharacter.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "MotionWarpingComponent.h"
#include "AbilitySystemComponent.h"

UGA_EnemyTurn::UGA_EnemyTurn()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    FGameplayTag TurnTag = FGameplayTag::RequestGameplayTag(TEXT("State.Turn"), false);
    if (TurnTag.IsValid())
    {
        ActivationOwnedTags.AddTag(TurnTag); // 어빌리티 실행 동안 캐릭터에게 실제 태그 부여!
    }

    FGameplayTag PainTag = FGameplayTag::RequestGameplayTag(TEXT("State.Pain"), false);
    if (PainTag.IsValid())
    {
        CancelAbilitiesWithTag.AddTag(PainTag);
    }

    FGameplayTag BrokenTag = FGameplayTag::RequestGameplayTag(TEXT("State.Broken"), false);
    if (BrokenTag.IsValid())
    {
        CancelAbilitiesWithTag.AddTag(BrokenTag);
    }

    FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
    if (AttackTag.IsValid())
    {
        ActivationBlockedTags.AddTag(AttackTag); // 공격 중일 때는 회전 어빌리티 실행 절대 금지!
    }
}

void UGA_EnemyTurn::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AEnemyCharacter* Boss = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
    if (!Boss)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AActor* TargetActor = nullptr;
    if (AAIController* AIC = Cast<AAIController>(Boss->GetController()))
    {
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
    UAnimMontage* MontageToPlay = (Angle >= 0.0f) ? TurnRightMontage : TurnLeftMontage;

    if (!MontageToPlay)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    FString TurnDir = (MontageToPlay == TurnRightMontage) ? TEXT("👉 [오른쪽 턴]") : TEXT("👈 [왼쪽 턴]");
    UE_LOG(LogTemp, Warning, TEXT("%s 회전 어빌리티 시작! (시작 전 상대 각도: %.1f° | 보스 시작 Yaw: %.1f°)"), *TurnDir, Angle, Boss->GetActorRotation().Yaw);

    // 3. Motion Warping 회전값 설정 (1회 회전당 최대 ±90도로 제한하여 170도일 때 묵직하게 2회 회전 수행)
    UMotionWarpingComponent* MotionWarpingComp = Boss->FindComponentByClass<UMotionWarpingComponent>();
    if (MotionWarpingComp)
    {
        float ClampedAngle = FMath::Clamp(Angle, -90.0f, 90.0f);
        FRotator TargetRotation = Boss->GetActorRotation();
        TargetRotation.Yaw += ClampedAngle;
        TargetRotation.Pitch = 0.0f;
        TargetRotation.Roll = 0.0f;

        MotionWarpingComp->AddOrUpdateWarpTargetFromTransform(WarpTargetName, FTransform(TargetRotation, Boss->GetActorLocation()));
    }

    // 4. 몽타주 재생
    UAnimInstance* AnimInstance = Boss->GetMesh() ? Boss->GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    float Duration = AnimInstance->Montage_Play(MontageToPlay);
    if (Duration <= 0.0f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    FOnMontageEnded EndedDelegate;
    EndedDelegate.BindUObject(this, &UGA_EnemyTurn::OnMontageEnded);
    AnimInstance->Montage_SetEndDelegate(EndedDelegate, MontageToPlay);
}

void UGA_EnemyTurn::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    AEnemyCharacter* Boss = Cast<AEnemyCharacter>(GetAvatarActorFromActorInfo());
    UE_LOG(LogTemp, Warning, TEXT("🏁 [GA_EnemyTurn] 회전 어빌리티 종료! (취소됨?: %s | 현재 보스 Yaw: %.1f°)"), bWasCancelled ? TEXT("예(강제 취소됨!)") : TEXT("아니오(정상완료)"), Boss ? Boss->GetActorRotation().Yaw : 0.0f);

    if (Boss && bWasCancelled && Boss->GetMesh() && Boss->GetMesh()->GetAnimInstance())
    {
        Boss->GetMesh()->GetAnimInstance()->Montage_Stop(0.2f);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_EnemyTurn::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted);
}
