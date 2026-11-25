#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSetTypes.h" // EWeaponStance, FWeaponAnimSet 등
#include "CoreMinimal.h"
#include "NonAnimInstance.generated.h"


/**
 * 프로젝트 공용 AnimInstance
 * - ABP_Non 이 이 클래스를 부모로 사용
 */
UCLASS()
class NON_API UNonAnimInstance : public UAnimInstance {
  GENERATED_BODY()

public:
  UNonAnimInstance();

  // ===== 이동 관련 값 =====
  UPROPERTY(BlueprintReadOnly, Category = "Movement")
  float MoveForward = 0.f; // -1(뒤) ~ +1(앞)

  UPROPERTY(BlueprintReadOnly, Category = "Movement")
  float MoveRight = 0.f; // -1(좌) ~ +1(우)

  // ===== 상태 값(ABP_Non에서 직접 참조) =====
  UPROPERTY(BlueprintReadOnly, Category = "Movement",
            meta = (AllowPrivateAccess = "true"))
  float GroundSpeed = 0.f;

  UPROPERTY(BlueprintReadOnly, Category = "Movement",
            meta = (AllowPrivateAccess = "true"))
  float MovementDirection = 0.f;

  UPROPERTY(BlueprintReadOnly, Category = "Movement",
            meta = (AllowPrivateAccess = "true"))
  bool bIsInAir = false;

  UPROPERTY(BlueprintReadOnly, Category = "Movement",
            meta = (AllowPrivateAccess = "true"))
  bool bIsAccelerating = false;

  // 풀바디 강제 (Dodge, KnockDown 등)
  UPROPERTY(BlueprintReadOnly, Category = "State")
  bool bForceFullBody = false;

  // 가드 중 여부
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Guard",
            meta = (AllowPrivateAccess = "true"))
  bool bGuarding = false;

  // 가드 방향
  UPROPERTY(BlueprintReadOnly, Category = "Guard",
            meta = (AllowPrivateAccess = "true"))
  float GuardDirection = 0.f;

  // 🛡️ 가드 중 피격 경직(가드 임팩트 몽타주 재생 중) 여부
  UPROPERTY(BlueprintReadOnly, Category = "Guard")
  bool bGuardImpact = false;

  // 무장/스탠스
  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "State")
  bool bArmed = false;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "State")
  EWeaponStance WeaponStance = EWeaponStance::Unarmed;

  // 잡힘 상태 및 Foot IK 알파 (C++ 틱에서 안전하게 자동 계산!)
  UPROPERTY(BlueprintReadOnly, Category = "State")
  bool bIsGrabbed = false;

  UPROPERTY(BlueprintReadOnly, Category = "State")
  float IKFootAlpha = 1.0f;

  // 하체를 이동 애니메이션으로 섞는 정도 (0 = 몽타주 전신, 1 = 하체는 이동 애니메이션 + 상체만 몽타주). 발도·납도 슬롯 블렌드에 사용.
  // 발도·납도 중: 본인/서버는 몽타주 루트 이동이 꺼졌는지, 다른 플레이어 화면은 서버가 복제한 이동 여부로 판단 (속도는 안 봄)
  UPROPERTY(BlueprintReadOnly, Category = "Movement")
  float LocomotionLowerBodyAlpha = 0.f;

  // LocomotionLowerBodyAlpha가 목표값으로 따라가는 속도 (클수록 빨리 넘어감)
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
  float LocomotionLowerBodyBlendSpeed = 10.f;

public:
  // 외부(캐릭터/어빌리티)에서 가드 on/off 설정용
  UFUNCTION(BlueprintCallable, Category = "Guard")
  void SetGuarding(bool bNewGuarding) { bGuarding = bNewGuarding; }

protected:
  // 틱마다 ABP_Non에서 쓰는 상태값 갱신
  virtual void NativeInitializeAnimation() override;
  virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
  // 내부 계산 함수
  void RefreshMovementStates(float DeltaSeconds);
};
