#include "Animation/ANS_IgnorePawnCollision.h"
#include "GameFramework/Character.h"
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

	if (!TargetActor)
	{
		TargetActor = UGameplayStatics::GetPlayerCharacter(OwnerActor->GetWorld(), 0);
	}

	if (TargetActor && TargetActor != OwnerActor)
	{
		CachedTarget = TargetActor;

		if (ACharacter* OwnerChar = Cast<ACharacter>(OwnerActor))
		{
			if (UCapsuleComponent* Capsule = OwnerChar->GetCapsuleComponent())
			{
				Capsule->IgnoreActorWhenMoving(TargetActor, true);
			}
		}
	}
}

void UANS_IgnorePawnCollision::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (CachedOwner.IsValid() && CachedTarget.IsValid())
	{
		if (ACharacter* OwnerChar = Cast<ACharacter>(CachedOwner.Get()))
		{
			if (UCapsuleComponent* Capsule = OwnerChar->GetCapsuleComponent())
			{
				Capsule->IgnoreActorWhenMoving(CachedTarget.Get(), false);
			}
		}
	}

	CachedOwner.Reset();
	CachedTarget.Reset();
}
