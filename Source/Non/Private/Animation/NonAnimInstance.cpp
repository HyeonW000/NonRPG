#include "Animation/NonAnimInstance.h"
#include "Character/NonCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UNonAnimInstance::NativeInitializeAnimation()
{
    OwnerChar = Cast<ANonCharacterBase>(TryGetPawnOwner());
}

void UNonAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!OwnerChar || !OwnerChar->GetWorld() || OwnerChar->GetWorld()->IsPreviewWorld())
    {
        // 미리보기/무효일 땐 안전값
        bIsInAir = false;
        bIsAccelerating = false;
        return;
    }

    // 이동 정보
    const FVector Vel = OwnerChar->GetVelocity();
    GroundSpeed = Vel.Size2D();

    // 공중/가속 여부
    if (const UCharacterMovementComponent* Move = OwnerChar->GetCharacterMovement())
    {
        bIsInAir = Move->IsFalling();

        // 현재 프레임의 가속 벡터 기준 (UE 표준 방식)
        const FVector Accel = Move->GetCurrentAcceleration();
        // 너무 작은 값 노이즈 제거 + 공중일 땐 보통 가속 판정 제외
        bIsAccelerating = (!Accel.IsNearlyZero(1e-3f) && !bIsInAir);
    }
    else
    {
        bIsInAir = false;
        bIsAccelerating = false;
    }


    // 이동 방향 (전방 대비 각도)
    const FVector Fwd = OwnerChar->GetActorForwardVector();
    const float Dot = FVector::DotProduct(Fwd.GetSafeNormal2D(), Vel.GetSafeNormal2D());
    const float CrossZ = FVector::CrossProduct(Fwd.GetSafeNormal2D(), Vel.GetSafeNormal2D()).Z;
    MovementDirection = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.f, 1.f))) * (CrossZ < 0 ? -1.f : 1.f);
    if (GroundSpeed < 5.f) { MovementDirection = 0.f; }

    // 무기/가드 상태 동기화
    bArmed = OwnerChar->IsArmed();
    WeaponStance = OwnerChar->GetWeaponStance();
    bGuarding = OwnerChar->IsGuarding();
    GuardDirection = bGuarding ? OwnerChar->GuardDirAngle : 0.f;
}

/** ================= Getter 구현 ================= */

UBlendSpace* UNonAnimInstance::GetIdleRunBlendSpace() const
{
    return (ClassSet) ? ClassSet->Common.BS_IdleRun : nullptr;
}

UAnimMontage* UNonAnimInstance::GetDeathMontage() const
{
    return (ClassSet) ? ClassSet->Common.DeathMontage : nullptr;
}

// FHitReactSet에는 Generic만 존재함 → Light/Heavy 모두 Generic 반환으로 통일
UAnimMontage* UNonAnimInstance::GetHitReactMontage_Light() const
{
    return (ClassSet) ? ClassSet->Common.HitReact.Generic : nullptr;
}

UAnimMontage* UNonAnimInstance::GetHitReactMontage_Heavy() const
{
    return (ClassSet) ? ClassSet->Common.HitReact.Generic : nullptr;
}

const FWeaponAnimSet& UNonAnimInstance::GetWeaponAnimSet() const
{
    static FWeaponAnimSet Dummy; // null-safe fallback
    if (!WeaponSet)
    {
        UE_LOG(LogTemp, Error, TEXT("[Anim] WeaponSet is NULL"));
        return Dummy;
    }

    const FWeaponAnimSet& R = WeaponSet->GetSetByStance(WeaponStance);
    UE_LOG(LogTemp, Verbose, TEXT("[Anim] GetWeaponAnimSet: Stance=%s | Equip=%s | Unequip=%s | C1=%s"),
        *UEnum::GetValueAsString(WeaponStance),
        *GetNameSafe(R.Equip),
        *GetNameSafe(R.Unequip),
        *GetNameSafe(R.Attacks.Combo1));

    return R;
}

// Equip/Unequip은 FWeaponAnimSet의 1단계 필드
UAnimMontage* UNonAnimInstance::GetEquipMontage() const
{
    return GetWeaponAnimSet().Equip;
}

// “Sheathe” 네이밍은 프로젝트 쪽 헤더에 이미 노출되어 있어 시그니처 유지 → 내부적으로 Unequip 매핑
UAnimMontage* UNonAnimInstance::GetSheatheMontage() const
{
    return GetWeaponAnimSet().Unequip;
}

// 회피/가드는 Defense 세트 하위
UAnimMontage* UNonAnimInstance::GetDodgeMontage() const
{
    return GetWeaponAnimSet().Defense.Dodge;
}

UAnimMontage* UNonAnimInstance::GetGuardMontage() const
{
    return GetWeaponAnimSet().Defense.Guard;
}

// 콤보는 Attacks 하위 Combo1/2/3
UAnimMontage* UNonAnimInstance::GetComboMontage(int32 ComboIndex) const
{
    const FWeaponAnimSet& Set = GetWeaponAnimSet();
    switch (ComboIndex)
    {
    case 1: return Set.Attacks.Combo1;
    case 2: return Set.Attacks.Combo2;
    case 3: return Set.Attacks.Combo3;
    default: return nullptr;
    }
}
