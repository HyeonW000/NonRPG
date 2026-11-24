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

    // 후방 공격은 등 뒤 대상을 치는 공격이라 워프 타깃을 갱신하지 않음 (판정은 스킬 태그 기준)
    const UAnimMontage* PlayingMontage = Cast<UAnimMontage>(Animation);
    if (!PlayingMontage)
    {
        PlayingMontage = Enemy->GetCurrentMontage();
    }
    if (Enemy->IsBackAttackMontage(PlayingMontage))
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

        // 블랙보드에 대상이 없으면 복제되는 추격 대상으로 대체 (첫 플레이어로 대체하면 협동에서 항상 호스트를 노림)
        if (!TargetActor)
        {
            TargetActor = Enemy->GetAggroTarget();
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
