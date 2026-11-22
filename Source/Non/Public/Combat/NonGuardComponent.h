#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimSetTypes.h"
#include "NonGuardComponent.generated.h"

class ANonCharacterBase;
class UAnimMontage;

// 8방향 (가드 이동 방향 판정용)
UENUM(BlueprintType)
enum class EGuardDir8 : uint8 { F, FR, R, BR, B, BL, L, FL };

// 무기 스탠스별 (피격 태그 -> 가드 몽타주) 매핑
USTRUCT(BlueprintType)
struct FStanceGuardHitMontageMap {
  GENERATED_BODY()

  // 태그별 가드 몽타주 지정 (예: Effect.Hit.Light -> AM_GS_Guard_Hit)
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard")
  TMap<FGameplayTag, TObjectPtr<UAnimMontage>> TagMontageMap;
};

/**
 * 플레이어 가드 담당 컴포넌트.
 * - 가드 상태(리플리케이트), 가드 중 이동 속도/방향
 * - 피격 시 가드 성공 판정(ResolveIncomingHit)과 가드 피격 몽타주
 * 캐릭터(ANonCharacterBase)는 기존 함수 이름(IsGuarding 등)을 유지하고 이 컴포넌트로 위임한다.
 * 가드 중 제자리 턴(Step Turn)은 캐릭터 회전 로직과 묶여 있어 아직 캐릭터에 있음.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class NON_API UNonGuardComponent : public UActorComponent {
  GENERATED_BODY()

public:
  UNonGuardComponent();

  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;

  // ── 상태 조회 ──
  UFUNCTION(BlueprintPure, Category = "Guard")
  bool IsGuarding() const { return bGuarding; }

  UFUNCTION(BlueprintPure, Category = "Guard")
  EGuardDir8 GetGuardDir8() const { return GuardDir8; }

  UFUNCTION(BlueprintPure, Category = "Guard")
  float GetGuardDirAngle() const { return GuardDirAngle; }

  // 최근 피격의 가드 성공 여부 (전방 성공=true, 후방/노가드=false)
  UFUNCTION(BlueprintPure, Category = "Guard")
  bool IsLastGuardSuccessful() const { return bLastGuardSuccess; }

  UFUNCTION(BlueprintPure, Category = "Guard")
  bool IsGuardImpactActive() const { return bIsGuardImpactActive; }

  // ── 동작 ──
  void StartGuard();
  void StopGuard(bool bForceRelease = false);

  // 가드 중 이동 방향/속도 갱신 (캐릭터 Tick에서 호출)
  void UpdateGuardDirAndSpeed();

  /**
   * 피격 시 가드 판정. 가드 성공이면 피해를 줄이고 가드 피격 몽타주를 재생한다.
   * @param InOutDamage  들어온 피해. 가드 성공 시 GuardDamageMultiplier가 곱해짐
   * @param bFrontalHit  공격자가 전방(가드 가능 범위)에 있는지
   * @param ReactionTag  공격의 피격 태그 (가드 몽타주 선택용)
   * @param bOutParried 패링 성공 시 true (피해 0, SP 소모 0). 호출자는 피해 적용을 건너뛴다.
   * @return 가드 성공(막음)이면 true. false면 호출자가 일반 피격 처리.
   */
  bool ResolveIncomingHit(float &InOutDamage, bool bFrontalHit,
                          FGameplayTag ReactionTag, AActor *Attacker = nullptr,
                          bool *bOutParried = nullptr);

  // 방패(보조 무기 슬롯) 착용 여부. 패링 가능 조건 (설계서 5장 16)
  UFUNCTION(BlueprintPure, Category = "Guard|Parry")
  bool HasShieldEquipped() const;

  UFUNCTION(BlueprintCallable, Category = "Guard")
  void PlayGuardHitMontage(FGameplayTag ImpactTag = FGameplayTag(), bool bParry = false);

  // ── 설정값 (BP_NonCharacterBase의 GuardComponent에서 편집) ──
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Speed")
  float GuardWalkSpeed = 150.f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Speed")
  float GuardBackSpeed = 150.f;

  /** 가드 상태일 때 감속도 (정지 시 잔여 속도에 의한 미끄러짐 방지) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Speed")
  float GuardBrakingDeceleration = 4000.f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Angle")
  float GuardBackEnterDeg = 135.f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Angle")
  float GuardBackExitDeg = 120.f;

  /** 가드 성공 시 실제로 들어가는 피해 비율 (0.2 = 20%) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Damage",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float GuardDamageMultiplier = 0.2f;

  /** 가드 성공 시 원래 피해량 대비 스태미나(SP) 차감 배율 (설계서 5장 15) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Stamina",
            meta = (ClampMin = "0.0"))
  float GuardStaminaCostMultiplier = 0.5f;

  /** 가드 피격으로 스태미나 0 도달 시 가드 브레이크(State.Groggy) 지속 시간 (설계서 5장 15: 1.5초) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Stamina",
            meta = (ClampMin = "0.1"))
  float GuardBreakDuration = 1.5f;

  /** 패링 인정 시간: 가드 입력(서버 수신) 후 이 시간 안에 정면 피격이면 패링 (설계서 5장 16: 0.2초) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Parry",
            meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float ParryWindow = 0.2f;

  /** 연타 방지: 패링 인정 시간이 끝난 뒤(실패 시) 이 시간 동안은 가드를 다시 눌러도 패링 창이 열리지 않음 (일반 가드는 됨) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Parry",
            meta = (ClampMin = "0.0", ClampMax = "3.0"))
  float ParryFailLockout = 0.5f;

  /** 패링 성공 시 재생할 몽타주. 비어 있으면 가드 피격 몽타주로 대체 */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Parry")
  TObjectPtr<UAnimMontage> ParryMontage;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Animation")
  TMap<EWeaponStance, FStanceGuardHitMontageMap> StanceGuardHitMap;

  // 스탠스 맵 미지정 시 Fallback
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Guard|Animation")
  TObjectPtr<UAnimMontage> GuardHitMontage;

protected:
  UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_IsGuarding, Category = "Guard")
  bool bGuarding = false;

  UPROPERTY(VisibleAnywhere, Category = "Guard")
  EGuardDir8 GuardDir8 = EGuardDir8::F;

  UPROPERTY(VisibleAnywhere, Category = "Guard")
  float GuardDirAngle = 0.f;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard")
  bool bLastGuardSuccess = false;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard")
  bool bIsGuardImpactActive = false;

  // ── 패링 (서버 전용, 복제 안 함) ──
  // 이번 가드 입력으로 열린 패링 창 시작 시각. 창이 없으면 -1
  float ParryWindowStartTime = -1.f;
  // 이 시각 전에는 새 패링 창을 열지 않음 (연타 방지)
  float ParryLockoutUntil = 0.f;

  // 가드 시작 시 패링 창을 열지 판단 (서버)
  void TryOpenParryWindow();
  bool IsInParryWindow() const;

  // 후방 이동 구간 히스테리시스 (이전에는 함수 static이라 모든 캐릭터가 공유하던 값)
  bool bInBackZone = false;

  FTimerHandle GuardImpactTimerHandle;
  FTimerHandle GuardBreakTimerHandle;

  UFUNCTION()
  void OnRep_IsGuarding();

  UFUNCTION(Server, Reliable)
  void ServerSetGuarding(bool bEnable);

  UFUNCTION(NetMulticast, Reliable)
  void Multicast_PlayGuardHitMontage(UAnimMontage *TargetMontage);

  // 가드 진입/해제 시 이동 속도·몽타주 처리 (StartGuard/StopGuard/OnRep 공용)
  void ApplyGuardEnterEffects();
  void ApplyGuardExitEffects();

  void PlayMontageOnOwner(UAnimMontage *Montage);
  void ClearGuardImpactState();

  ANonCharacterBase *GetOwnerCharacter() const;
};
