#include "Animation/EnemyAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/EnemyCharacter.h"
#include "Animation/BlendSpace.h"
#include "Animation/EnemyAnimSet.h"

void UEnemyAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    OwnerChar = Cast<ACharacter>(TryGetPawnOwner());
    if (OwnerChar.IsValid())
    {
        MoveComp = OwnerChar->GetCharacterMovement();
    }
    PullAnimSetFromOwner();
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
            PullAnimSetFromOwner();
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

void UEnemyAnimInstance::PullAnimSetFromOwner()
{
    if (!OwnerChar.IsValid()) return;

    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(OwnerChar.Get()))
    {
        if (const UEnemyAnimSet* Set = Enemy->GetAnimSet())
        {
            LocomotionBS = Set->Locomotion;
        }
    }
}
