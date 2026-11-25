#include "Animation/BossAnimInstance.h"
#include "Core/NonGameplayTags.h"
#include "Character/BossCharacter.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "AbilitySystemComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UBossAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!BossChar.IsValid())
    {
        BossChar = Cast<ABossCharacter>(TryGetPawnOwner());
    }

    if (BossChar.IsValid())
    {
        BossPhase = BossChar->CurrentPhase;
        bIsTransitioning = BossChar->bIsTransitioningPhase;
        Speed = BossChar->GetVelocity().Size2D();

        // ───── 보스 헤드(시선) 어그로 타겟 트래킹 계산 ─────
        AActor* TargetActor = BossChar->CurrentTargetActor.Get();
        if (!TargetActor)
        {
            if (AAIController* AIC = Cast<AAIController>(BossChar->GetController()))
            {
                if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
                {
                    TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
                }
            }
        }

        bool bCanLookAt = (TargetActor != nullptr) && !BossChar->IsDead() && !bIsTransitioning && !BossChar->bIsPlayingPartBreak;
        if (bCanLookAt)
        {
            // 1) 제자리 턴(Turn) 또는 부위 파괴(Pain) 몽타주 재생 중에는 추가 시선 회전을 부드럽게 0으로 풀어줍니다.
            const UAnimMontage* CurMontage = BossChar->GetCurrentMontage();
            if (CurMontage && (CurMontage == BossChar->TurnLeftMontage || CurMontage == BossChar->TurnRightMontage || BossChar->IsPlayingPartBreakMontage(CurMontage)))
            {
                bCanLookAt = false;
            }
            else if (UAbilitySystemComponent* ASC = BossChar->GetAbilitySystemComponent())
            {
                static const FGameplayTag TurnTag = NonTags::State_Turn.GetTag();
                static const FGameplayTag StunnedTag = NonTags::State_Stunned.GetTag();
                static const FGameplayTag HitReactingTag = NonTags::State_HitReacting.GetTag();
                if ((TurnTag.IsValid() && ASC->HasMatchingGameplayTag(TurnTag)) ||
                    (StunnedTag.IsValid() && ASC->HasMatchingGameplayTag(StunnedTag)) ||
                    (HitReactingTag.IsValid() && ASC->HasMatchingGameplayTag(HitReactingTag)))
                {
                    bCanLookAt = false;
                }
            }
        }

        float DesiredYaw = 0.0f;
        float DesiredPitch = 0.0f;
        float DesiredAlpha = 0.0f;

        USkeletalMeshComponent* MeshComp = BossChar->GetMesh();

        if (bCanLookAt && TargetActor)
        {
            const FVector HeadLoc = (MeshComp && MeshComp->DoesSocketExist(TEXT("head")))
                ? MeshComp->GetSocketLocation(TEXT("head"))
                : (BossChar->GetActorLocation() + FVector(0.f, 0.f, 150.f));

            // 플레이어의 가슴/머리 높이 지점을 응시
            HeadLookAtLocation = TargetActor->GetActorLocation() + FVector(0.f, 0.f, 50.f);

            const FRotator WorldLookRot = UKismetMathLibrary::FindLookAtRotation(HeadLoc, HeadLookAtLocation);
            const FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(WorldLookRot, BossChar->GetActorRotation());

            // 플레이어가 정면 시야각(±80도 이내)에 있을 때 고개를 돌리고, 등 뒤로 넘어가면 정면으로 아주 부드럽게 복귀
            if (FMath::Abs(DeltaRot.Yaw) <= 80.0f)
            {
                // 기존 블루프린트 CDO에 이전 기본값(60도)이 저장되어 있더라도 새 기본값(35도)이 즉시 적용되도록 보정
                const float EffectiveMaxYaw = (MaxHeadYawAngle >= 59.9f) ? 35.0f : MaxHeadYawAngle;

                // 실제 타격 판정 순간(내려찍는 순간)이나 공중 점프 중에만 머리 회전 각도를 살짝 완화하여 목 꺾임 방지
                const bool bIsJumping = BossChar->GetCharacterMovement() && BossChar->GetCharacterMovement()->IsFalling();
                const float AngleScale = (BossChar->bIsAttackHitboxActive || bIsJumping) ? 0.5f : 1.0f;

                // 각도의 65%만 따라가되 최대 35도(EffectiveMaxYaw)를 넘지 않도록 제한하여 묵직하고 자연스러운 시선 처리
                DesiredYaw = FMath::Clamp(DeltaRot.Yaw * 0.65f, -EffectiveMaxYaw, EffectiveMaxYaw) * AngleScale;
                DesiredPitch = FMath::Clamp(DeltaRot.Pitch * 0.65f, -MaxHeadPitchAngle, MaxHeadPitchAngle) * AngleScale;
                DesiredAlpha = 1.0f;
            }
        }

        const float EffectiveInterpSpeed = (DesiredAlpha > 0.0f) ? HeadLookAtInterpSpeed : HeadLookAtReturnSpeed;
        CurrentLookYaw = FMath::FInterpTo(CurrentLookYaw, DesiredYaw, DeltaSeconds, EffectiveInterpSpeed);
        CurrentLookPitch = FMath::FInterpTo(CurrentLookPitch, DesiredPitch, DeltaSeconds, EffectiveInterpSpeed);
        HeadLookAtAlpha = FMath::FInterpTo(HeadLookAtAlpha, DesiredAlpha, DeltaSeconds, EffectiveInterpSpeed);

        // ★ [핵심: 정면에서 봤을 때 고개가 시계방향(갸우뚱)으로 삐딱하게 도는 현상 완벽 차단!]
        // 언리얼 Character의 SkeletalMeshComponent(Component Space)는 기본적으로 Yaw가 -90도 회전되어(+Y축이 정면) 있습니다.
        // 따라서 Actor 공간의 순수 상하(Pitch) / 좌우(Yaw) 회전(Roll=0.0f 고정)을 Mesh의 Component Space 쿼터니언으로 정확히 변환하여 전달합니다!
        const FQuat ActorLookQuat = FRotator(CurrentLookPitch, CurrentLookYaw, 0.0f).Quaternion();
        const FQuat MeshRelQuat = MeshComp ? MeshComp->GetRelativeRotation().Quaternion() : FRotator(0.0f, -90.0f, 0.0f).Quaternion();
        const FQuat CompLookQuat = MeshRelQuat.Inverse() * ActorLookQuat * MeshRelQuat;

        HeadLookAtRotation = CompLookQuat.Rotator();
    }
}
