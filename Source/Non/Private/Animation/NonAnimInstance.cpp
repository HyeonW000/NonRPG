#include "Animation/NonAnimInstance.h"
#include "Animation/AnimSet_Weapon.h"   // class
#include "Animation/AnimSet_Common.h"   // class
#include "Animation/AnimSetTypes.h"     // structs/enums

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Kismet/KismetSystemLibrary.h"

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
    RefreshMovementStates(DeltaSeconds);
}

void UNonAnimInstance::RefreshMovementStates(float /*DeltaSeconds*/)
{
    APawn* OwnerPawn = TryGetPawnOwner();
    ACharacter* Char = Cast<ACharacter>(OwnerPawn);
    const UCharacterMovementComponent* Move = Char ? Char->GetCharacterMovement() : nullptr;

    // 속도/가속/점프 여부
    const FVector Vel = OwnerPawn ? OwnerPawn->GetVelocity() : FVector::ZeroVector;
    GroundSpeed = FVector(Vel.X, Vel.Y, 0.f).Size();

    bIsInAir = Move ? Move->IsFalling() : false;
    bIsAccelerating = Move ? (Move->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER) : false;

    // 이동 방향(AnimInstance 제공 함수 사용)
    const FRotator ActorRot = OwnerPawn ? OwnerPawn->GetActorRotation() : FRotator::ZeroRotator;
    MovementDirection = CalculateDirection(Vel, ActorRot);

    // Guard 방향: 일단 MovementDirection과 동일하게 제공 (원하면 Aim 기반으로 변경)
    GuardDirection = MovementDirection;
}

const FWeaponAnimSet& UNonAnimInstance::GetWeaponAnimSet() const
{
    static FWeaponAnimSet Dummy; // null-safe
    if (!WeaponSet)
    {
        UE_LOG(LogTemp, Error, TEXT("[Anim] WeaponSet is NULL"));
        return Dummy;
    }

    const FWeaponAnimSet& R = WeaponSet->GetSetByStance(WeaponStance);
    UE_LOG(LogTemp, Verbose, TEXT("[Anim] WeaponSet: Stance=%s | Equip=%s | Unequip=%s | C1=%s"),
        *UEnum::GetValueAsString(WeaponStance),
        *GetNameSafe(R.Equip),
        *GetNameSafe(R.Unequip),
        *GetNameSafe(R.Attacks.Combo1));
    return R;
}

UAnimMontage* UNonAnimInstance::GetEquipMontage() const
{
    return GetWeaponAnimSet().Equip;
}

UAnimMontage* UNonAnimInstance::GetSheatheMontage() const
{
    return GetWeaponAnimSet().Unequip;
}

UAnimMontage* UNonAnimInstance::GetDeathMontage() const
{
    if (CommonSet && CommonSet->Common.DeathMontage)
        return CommonSet->Common.DeathMontage;
    return nullptr;
}

UAnimMontage* UNonAnimInstance::GetCommonHitReact() const
{
    if (CommonSet && CommonSet->Common.HitReact_Generic)
        return CommonSet->Common.HitReact_Generic;
    return nullptr;
}

// ───────────── NEW: Dodge / HitReact (무기별) ─────────────

UAnimMontage* UNonAnimInstance::GetDodgeByDirIndex(int32 DirIdx) const
{
    const FWeaponAnimSet& R = GetWeaponAnimSet();
    if (UAnimMontage* M = R.Dodge.GetByIndex(DirIdx))
        return M;

    // 폴백: Unarmed의 해당 방향
    if (WeaponSet)
    {
        const FWeaponAnimSet& Fallback = WeaponSet->GetSetByStance(EWeaponStance::Unarmed);
        return Fallback.Dodge.GetByIndex(DirIdx);
    }
    return nullptr;
}

UAnimMontage* UNonAnimInstance::GetHitReact_Light() const
{
    const FWeaponAnimSet& R = GetWeaponAnimSet();
    if (R.HitReact_Light) return R.HitReact_Light;
    return GetCommonHitReact();
}

UAnimMontage* UNonAnimInstance::GetHitReact_Heavy() const
{
    const FWeaponAnimSet& R = GetWeaponAnimSet();
    if (R.HitReact_Heavy) return R.HitReact_Heavy;
    return GetCommonHitReact();
}