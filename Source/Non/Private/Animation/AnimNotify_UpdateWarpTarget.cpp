#include "Animation/AnimNotify_UpdateWarpTarget.h"
#include "Character/EnemyCharacter.h"
#include "Character/BossCharacter.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "MotionWarpingComponent.h"

void UAnimNotify_UpdateWarpTarget::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp) return;

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner());
    if (!Enemy) return;

    if (Animation && Animation->GetName().Contains(TEXT("Back"), ESearchCase::IgnoreCase))
    {
        return;
    }

    if (ABossCharacter* Boss = Cast<ABossCharacter>(Enemy))
    {
        Boss->UpdateBossMotionWarpTarget();
        return;
    }

    if (UMotionWarpingComponent* MotionWarpingComp = Enemy->FindComponentByClass<UMotionWarpingComponent>())
    {
        AActor* TargetActor = nullptr;
        if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
        {
            if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
            {
                TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
            }
        }

        if (!TargetActor && Enemy->GetWorld())
        {
            if (APlayerController* PC = Enemy->GetWorld()->GetFirstPlayerController())
            {
                TargetActor = PC->GetPawn();
            }
        }

        if (TargetActor)
        {
            FVector TargetLoc = TargetActor->GetActorLocation();
            FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(Enemy->GetActorLocation(), TargetLoc);
            TargetRot.Pitch = 0.0f;
            TargetRot.Roll = 0.0f;

            MotionWarpingComp->AddOrUpdateWarpTargetFromTransform(FName("LocationTarget"), FTransform(TargetRot, TargetLoc));
        }
    }
}
