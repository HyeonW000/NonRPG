#include "Animation/EnemyAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/EnemyCharacter.h"
#include "Animation/BlendSpace.h"

void UEnemyAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    OwnerChar = Cast<ACharacter>(TryGetPawnOwner());
    if (OwnerChar.IsValid())
    {
        MoveComp = OwnerChar->GetCharacterMovement();
    }
}

void UEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!OwnerChar.IsValid())
    {
        OwnerChar = Cast<ACharacter>(TryGetPawnOwner());
        if (OwnerChar.IsValid())
        {
            MoveComp = OwnerChar->GetCharacterMovement();
        }
    }

    if (!OwnerChar.IsValid()) return;

    const FVector Vel = OwnerChar->GetVelocity();
    Speed = FVector(Vel.X, Vel.Y, 0.f).Size();

    if (MoveComp.IsValid())
    {
        bInAir = MoveComp->IsFalling();
        bIsAccelerating = MoveComp->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER;
    }
}
