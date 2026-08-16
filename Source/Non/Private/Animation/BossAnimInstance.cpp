#include "Animation/BossAnimInstance.h"
#include "Character/BossCharacter.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"

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
    }
}
