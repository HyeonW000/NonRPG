#include "Animation/NonAnimInstance.h"
#include "Animation/AnimSetTypes.h"     // structs/enums
#include "Character/NonCharacterBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UNonAnimInstance::UNonAnimInstance()
{
}

void UNonAnimInstance::NativeInitializeAnimation()
{
    // 초기화 시점에 특별히 할 건 없음
}

void UNonAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    ACharacter* OwnerChar = Cast<ACharacter>(TryGetPawnOwner());
    if (!OwnerChar) return;

    // Non 캐릭터만 처리
    ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(OwnerChar);
    if (!NonChar) return;

    // 상태 동기화(기본)
    bArmed = NonChar->IsArmed();
    WeaponStance = NonChar->GetWeaponStance();
    bGuarding = NonChar->IsGuarding();
    // 풀바디 강제 플래그 전달
    bForceFullBody = NonChar->IsForceFullBody();

    // 이동 파생값 계산(기본형)
    RefreshMovementStates(DeltaSeconds);

    AimYawDelta = NonChar->AimYawDelta;
}

void UNonAnimInstance::RefreshMovementStates(float /*DeltaSeconds*/)
{
    APawn* OwnerPawn = TryGetPawnOwner();
    ACharacter* Char = Cast<ACharacter>(OwnerPawn);
    const UCharacterMovementComponent* Move = Char ? Char->GetCharacterMovement() : nullptr;

    // 1) 속도/가속/점프
    const FVector Vel = OwnerPawn ? OwnerPawn->GetVelocity() : FVector::ZeroVector;
    const FVector Vel2D = FVector(Vel.X, Vel.Y, 0.f);
    const float   Speed2D = Vel2D.Size();
    GroundSpeed = Speed2D;

    bIsInAir = Move ? Move->IsFalling() : false;
    bIsAccelerating = Move ? (Move->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER) : false;

    // 2) 기준 회전(카메라 기준 권장)
    const FRotator RefYaw = (Char && Char->GetController())
        ? Char->GetControlRotation()
        : OwnerPawn->GetActorRotation();

    const FRotator OnlyYaw(0.f, RefYaw.Yaw, 0.f);
    const FVector  Fwd = FRotationMatrix(OnlyYaw).GetUnitAxis(EAxis::X);
    const FVector  Rt = FRotationMatrix(OnlyYaw).GetUnitAxis(EAxis::Y);

    // 3) 전/후, 좌/우 성분 (-1..+1)
    const FVector Dir = (Speed2D > KINDA_SMALL_NUMBER) ? (Vel2D / Speed2D) : FVector::ZeroVector;
    MoveForward = FVector::DotProduct(Dir, Fwd);  // 세로축(Forward)
    MoveRight = FVector::DotProduct(Dir, Rt);   // 가로축(Right)
}