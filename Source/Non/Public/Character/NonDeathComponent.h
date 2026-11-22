#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimSetTypes.h"
#include "NonDeathComponent.generated.h"

class ANonCharacterBase;
class UAnimMontage;
class UGameplayEffect;

/**
 * 플레이어 사망·부활 담당.
 * - 사망 처리(이동/입력 차단, 사망 몽타주, 게임 오버 UI), 사망 포즈 고정
 * - 부활(HP/MP 회복, 메쉬/콜리전/입력/스탠스 복구), 부활 패널티(경험치 감소, 부활 후유증 GE)
 * - 무기 스탠스별 사망/부활 몽타주
 * 사망 플래그(bDied)는 IsDead() 등에서 널리 쓰여 ANonCharacterBase에 남아 있고,
 * 이 컴포넌트는 friend로 접근한다. 캐릭터는 기존 함수 이름(HandleDeath, Revive 등)을 유지하고 위임한다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class NON_API UNonDeathComponent : public UActorComponent {
  GENERATED_BODY()

public:
  UNonDeathComponent();

  // 캐릭터 사망 처리 (서버/클라 각자 호출됨 — 기존 흐름 유지)
  void HandleDeath();

  // 사망 몽타주 종료 후 자세 고정
  void FreezeDeathPose();

  // 부활 (제자리 / 다른 곳)
  void Revive(bool bInPlace);

  // 부활 시 경험치 감소 및 부활 후유증 GE 적용
  void ApplyResurrectionPenalty();

  UFUNCTION(BlueprintPure, Category = "Animation|Death")
  UAnimMontage *GetDeathMontage() const;

  UFUNCTION(BlueprintPure, Category = "Animation|Revive")
  UAnimMontage *GetReviveMontage() const;

  // ── 설정값 (BP_NonCharacterBase의 DeathComponent에서 편집) ──
  // 무기 스탠스별 사망 몽타주
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Death")
  TMap<EWeaponStance, TObjectPtr<UAnimMontage>> StanceDeathMontages;

  // 스탠스 맵 미지정 시 Fallback
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Death")
  TObjectPtr<UAnimMontage> DefaultDeathMontage;

  // 무기 스탠스별 부활 몽타주
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Revive")
  TMap<EWeaponStance, TObjectPtr<UAnimMontage>> StanceReviveMontages;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
  FName ResurrectionAmuletItemId = FName(TEXT("ResurrectionAmulet"));

  // 부활 시 경험치 감소 비율 (다음 레벨 필요 경험치 기준, 기본 3%)
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
  float ResurrectionExpPenaltyRatio = 0.03f;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
  TSubclassOf<UGameplayEffect> ResurrectionSicknessEffectClass;

protected:
  UFUNCTION(NetMulticast, Reliable)
  void Multicast_FreezeDeathPose();

  // 모든 클라이언트 화면에서 메쉬/애니메이션/콜리전/스탠스 복구
  UFUNCTION(NetMulticast, Reliable)
  void Multicast_OnRevive(bool bInPlace);

  ANonCharacterBase *GetOwnerCharacter() const;
};
