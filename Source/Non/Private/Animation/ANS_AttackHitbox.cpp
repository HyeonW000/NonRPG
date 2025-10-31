#include "Animation/ANS_AttackHitbox.h"
#include "AI/EnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UANS_AttackHitboxWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
    if (!MeshComp) return;
    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner()))
    {
        UE_LOG(LogTemp, Warning, TEXT("[ANS_AttackHitbox] Enable on %s (Dur=%.2f)"), *Enemy->GetName(), TotalDuration);
        Enemy->AttackHitbox_Enable();
    }
}

void UANS_AttackHitboxWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    if (!MeshComp) return;
    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner()))
    {
        UE_LOG(LogTemp, Warning, TEXT("[ANS_AttackHitbox] Disable on %s"), *Enemy->GetName());
        Enemy->AttackHitbox_Disable();
    }
}
