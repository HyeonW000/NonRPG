#include "Animation/ANS_IgnorePawnCollision.h"
#include "GameFramework/Character.h"
#include "Character/BossCharacter.h"
#include "Components/CapsuleComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"

UANS_IgnorePawnCollision::UANS_IgnorePawnCollision()
{
}

void UANS_IgnorePawnCollision::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp) return;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	CachedOwner = OwnerActor;

	AActor* TargetActor = nullptr;
	if (ABossCharacter* Boss = Cast<ABossCharacter>(OwnerActor))
	{
		TargetActor = Boss->CurrentTargetActor.Get();
	}

	if (!TargetActor)
	{
		if (ACharacter* CharOwner = Cast<ACharacter>(OwnerActor))
		{
			if (AAIController* AIC = Cast<AAIController>(CharOwner->GetController()))
			{
				if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
				{
					TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
				}
			}
		}
	}

	if (!TargetActor)
	{
		TargetActor = UGameplayStatics::GetPlayerCharacter(OwnerActor->GetWorld(), 0);
	}

	// ★ [핵심: AM_Golem_Atk3 등 점프 공격 직전 모션 워핑 타겟(LocationTarget) 최신화!]
	// AM_Golem_Atk3(땅 두 번 찍고 점프 공격)에서는 점프 직전(2.432초)에 ANS_IgnorePawnCollision이 시작되고 직후(2.440초)에 MotionWarping이 시작됩니다.
	// 따라서 여기서 LocationTarget을 플레이어의 현재 위치로 즉시 갱신해주면, 땅을 찍는 동안 이동한 플레이어를 향해 정확히 날아갑니다!
	if (ABossCharacter* Boss = Cast<ABossCharacter>(OwnerActor))
	{
		Boss->UpdateBossMotionWarpTarget(TargetActor);
	}

	if (ACharacter* OwnerChar = Cast<ACharacter>(OwnerActor))
	{
		if (UCapsuleComponent* Capsule = OwnerChar->GetCapsuleComponent())
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			if (TargetActor && TargetActor != OwnerActor)
			{
				CachedTarget = TargetActor;
				Capsule->IgnoreActorWhenMoving(TargetActor, true);
			}
		}
	}
}

void UANS_IgnorePawnCollision::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (CachedOwner.IsValid())
	{
		if (ACharacter* OwnerChar = Cast<ACharacter>(CachedOwner.Get()))
		{
			if (UCapsuleComponent* Capsule = OwnerChar->GetCapsuleComponent())
			{
				Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
				if (CachedTarget.IsValid())
				{
					Capsule->IgnoreActorWhenMoving(CachedTarget.Get(), false);
				}
			}
		}
	}

	CachedOwner.Reset();
	CachedTarget.Reset();
}
