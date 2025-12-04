#include "Animation/ANS_AttackHitbox.h"
#include "AI/EnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UANS_AttackHitboxWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    if (!MeshComp) return;
    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner()))
    {
        Enemy->AttackHitbox_Enable();
    }
}

void UANS_AttackHitboxWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    if (!MeshComp) return;
    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner()))
    {
        Enemy->AttackHitbox_Disable();
    }
}
