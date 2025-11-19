#include "Animation/NonAnimInstance.h"
#include "Animation/AnimSet_Weapon.h"   // class
#include "Animation/AnimSet_Common.h"   // class
#include "Animation/AnimSetTypes.h"     // structs/enums
#include "Character/NonCharacterBase.h"
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

    ACharacter* OwnerChar = Cast<ACharacter>(TryGetPawnOwner());
    if (!OwnerChar) return;

    // Non 캐릭터만 처리
    ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(OwnerChar);
    if (!NonChar) return;

    // 상태 동기화(기본)
    bArmed = NonChar->IsArmed();
    WeaponStance = NonChar->GetWeaponStance();
    bGuarding = NonChar->IsGuarding();

    // 이동 파생값 계산(기본형)
    RefreshMovementStates(DeltaSeconds);

    AimYawDelta = NonChar->AimYawDelta;

    UE_LOG(LogTemp, Verbose, TEXT("F: %.2f, R: %.2f, Spd: %.1f"), MoveForward, MoveRight, GroundSpeed);
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