#include "Animation/NonAnimInstance.h"
#include "Animation/AnimSetTypes.h" // structs/enums
#include "Character/NonCharacterBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"

UNonAnimInstance::UNonAnimInstance() {}

void UNonAnimInstance::NativeInitializeAnimation() {
  // 초기화 시점에 특별히 할 건 없음
}

void UNonAnimInstance::NativeUpdateAnimation(float DeltaSeconds) {
  Super::NativeUpdateAnimation(DeltaSeconds);

  ACharacter *OwnerChar = Cast<ACharacter>(TryGetPawnOwner());
  if (!OwnerChar)
    return;

  // C++ 틱에서 안전하게 State.Grabbed 태그 감지 및 IKFootAlpha 계산 (Accessed None 0% 사멸!)
  if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OwnerChar))
  {
    if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
    {
      FGameplayTag GrabbedTag = FGameplayTag::RequestGameplayTag(TEXT("State.Grabbed"), false);
      bIsGrabbed = (GrabbedTag.IsValid() && ASC->HasMatchingGameplayTag(GrabbedTag));
    }
  }

  // Non 캐릭터만 처리
  ANonCharacterBase *NonChar = Cast<ANonCharacterBase>(OwnerChar);
  if (!NonChar)
    return;

  // 상태 동기화(캐릭터의 정식 IsGuarding() 수치 100% 최우선 반영)
  bArmed = NonChar->IsArmed();
  WeaponStance = NonChar->GetWeaponStance();
  bGuarding = NonChar->IsGuarding();
  bGuardImpact = NonChar->IsGuardImpactActive();

  // 풀바디 강제 플래그 전달
  bForceFullBody = NonChar->IsForceFullBody();

  // 🛡️ [Fix] 풀바디 몽타주(루트 모션 발도, 구르기 등) 재생 중이거나 잡힌 상태일 때는
  // Control Rig 발 고정 IK를 해제(0.0)하여 발이 바닥에 묶여 덜덜덜 떨리는 현상을 방지하고 루트 모션을 온전히 보존!
  const bool bDisableFootIK = bIsGrabbed || bForceFullBody || NonChar->IsGuardTurning() || (GetRootMotionMontageInstance() != nullptr);
  const float TargetIKAlpha = bDisableFootIK ? 0.0f : 1.0f;
  IKFootAlpha = FMath::FInterpTo(IKFootAlpha, TargetIKAlpha, DeltaSeconds, 20.f);

  // 이동 파생값 계산(기본형)
  RefreshMovementStates(DeltaSeconds);

  // 가드 밀림 중: 캡슐만 미끄러지고 다리는 가드 서 있는 자세 유지 (BS가 뒷걸음으로 가지 않게)
  if (const UNonGuardComponent *Guard = NonChar->GetGuardComponent()) {
    if (Guard->IsGuardPushActive()) {
      GroundSpeed = 0.f;
      MoveForward = 0.f;
      MoveRight = 0.f;
      bIsAccelerating = false;
    }
  }
}

void UNonAnimInstance::RefreshMovementStates(float /*DeltaSeconds*/) {
  APawn *OwnerPawn = TryGetPawnOwner();
  ACharacter *Char = Cast<ACharacter>(OwnerPawn);
  const UCharacterMovementComponent *Move =
      Char ? Char->GetCharacterMovement() : nullptr;

  // 1) 속도/가속/점프
  const FVector Vel =
      OwnerPawn ? OwnerPawn->GetVelocity() : FVector::ZeroVector;
  const FVector Vel2D = FVector(Vel.X, Vel.Y, 0.f);
  const float Speed2D = Vel2D.Size();
  GroundSpeed = Speed2D;

  bIsInAir = Move ? Move->IsFalling() : false;
  bIsAccelerating =
      Move ? (Move->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER)
           : false;

  // 2) 기준 회전(카메라 기준 권장)
  const FRotator RefYaw = (Char && Char->GetController())
                              ? Char->GetControlRotation()
                              : OwnerPawn->GetActorRotation();

  const FRotator OnlyYaw(0.f, RefYaw.Yaw, 0.f);
  const FVector Fwd = FRotationMatrix(OnlyYaw).GetUnitAxis(EAxis::X);
  const FVector Rt = FRotationMatrix(OnlyYaw).GetUnitAxis(EAxis::Y);

  // 3) 전/후, 좌/우 성분 (-1..+1)
  const FVector Dir =
      (Speed2D > KINDA_SMALL_NUMBER) ? (Vel2D / Speed2D) : FVector::ZeroVector;
  MoveForward = FVector::DotProduct(Dir, Fwd); // 세로축(Forward)
  MoveRight = FVector::DotProduct(Dir, Rt);    // 가로축(Right)
}