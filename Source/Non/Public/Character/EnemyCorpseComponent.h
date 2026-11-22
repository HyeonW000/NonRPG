#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyCorpseComponent.generated.h"

class AEnemyCharacter;
class ANonCharacterBase;
class UMaterialInstanceDynamic;

// 사망 시 미리 추첨한 개인 전리품 한 줄 (서버 전용, 복제 안 함)
struct FCorpsePendingLootItem {
  FName ItemId;
  int32 Quantity = 0;
};

/**
 * 적의 등장/소멸 페이드, 시체 상호작용(개인 루팅), 사망 경험치 분배 담당.
 * 사망 연출(콜리전/래그돌/포즈 고정)은 AEnemyCharacter에 남아 있고,
 * 거기서 EnableCorpseInteraction() / DistributeExp()를 호출한다.
 * AEnemyCharacter는 기존 함수 이름(IsSpawnFading, Interact 등)을 유지하고 이 컴포넌트로 위임한다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class NON_API UEnemyCorpseComponent : public UActorComponent {
  GENERATED_BODY()

public:
  UEnemyCorpseComponent();

  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;

  // ── 페이드 ──
  // 메쉬 머티리얼에서 페이드용 MID 생성 (BeginPlay에서 호출)
  void InitFadeMaterials();

  // 등장 페이드 인 (0→1). Duration <= 0.01이면 즉시 표시
  void PlaySpawnFadeIn(float Duration);

  // 매 프레임 페이드 진행 (적 Tick에서 호출)
  void TickFade();

  UFUNCTION(BlueprintPure, Category = "Spawn|Fade")
  bool IsSpawnFading() const { return bFadeActive; }

  // ── 시체 / 루팅 ──
  // 사망 후 시체 상호작용 활성화 + 시체 수명 타이머 시작
  void EnableCorpseInteraction();

  // 개인 루팅: 이 플레이어에게 드롭 아이템 추첨·지급 (서버)
  void HandleInteract(ANonCharacterBase *Interactor);

  FText GetInteractLabel() const;
  void SetInteractHighlight(bool bEnable);

  UFUNCTION(BlueprintCallable, Category = "Interaction")
  void SetInteractionOutline(bool bEnable);

  UFUNCTION(BlueprintPure, Category = "Interaction")
  bool IsLootAvailable() const { return bLootAvailable; }

  // ── 보상 ──
  // 사망 시 호출 (서버): 루팅 대상자(솔로=처치자, 파티=거리 내 파티원) 확정 + 경험치 분배
  void OnOwnerDied(ANonCharacterBase *KillerChar, float ExpReward);

  // 파티 보너스 및 거리 제한 균등 분배 (서버)
  void DistributeExp(ANonCharacterBase *KillerChar, float ExpReward);

  // ── 설정값 ──
  // 페이드 중 AI/이동 정지 여부
  UPROPERTY(EditAnywhere, Category = "Spawn|Fade")
  bool bPauseAIWhileFading = true;

  // 페이드 인이 끝나면 AI/이동 자동 재개
  UPROPERTY(EditAnywhere, Category = "Spawn|Fade")
  bool bAutoResumeAfterFade = true;

  // 머티리얼의 페이드 스칼라 파라미터 이름 (1=보임, 0=투명)
  UPROPERTY(EditAnywhere, Category = "Spawn|Fade")
  FName FadeParamName = TEXT("Fade");

  UPROPERTY(EditAnywhere, Category = "Spawn|Fade", meta = (ClampMin = "0.5", ClampMax = "10.0"))
  float CorpseFadeOutDuration = 3.0f;

  // 루팅 안 해도 시체가 유지되는 시간
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
  float CorpseLifeTime = 120.f;

  // 루팅 후 시체 유지 시간 (0이면 바로 소멸 시작)
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
  float CorpseLifeTimeAfterLoot = 3.f;

  /** 경험치 공유 유효 반경 (cm, 3500 = 35m 이내 파티원만 획득) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Party")
  float PartyExpShareRadius = 3500.f;

  /** 파티 인원 1명 추가당 경험치 보너스 (+10% 기본) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Party")
  float PartyExpBonusPerMember = 0.10f;

protected:
  // 현재 루팅 가능한 상태인가 (네트워크 복제)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Interaction")
  bool bLootAvailable = false;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
  bool bLooted = false;

  // 이미 이 시체에서 전리품을 획득한 플레이어 목록 (개인 루팅)
  UPROPERTY()
  TArray<TWeakObjectPtr<ANonCharacterBase>> LootedCharacters;

  // 루팅 대상자 (사망 시 확정). 이 인원이 모두 루팅하면 CorpseLifeTimeAfterLoot 뒤 소멸
  UPROPERTY()
  TArray<TWeakObjectPtr<ANonCharacterBase>> EligibleLooters;

  // 사망 시 대상자별로 미리 추첨한 전리품 (서버 전용). 비어 있는 대상자는 처음부터 루팅 완료 처리
  TMap<TWeakObjectPtr<ANonCharacterBase>, TArray<FCorpsePendingLootItem>> PendingLoot;

  // 대상자별 드롭 추첨 + 받을 것이 없는 대상자/비대상자 화면에서 시체 상호작용 차단 (서버)
  void RollPersonalLoot();

  // EnableCorpseInteraction이 사망 시/사망 포즈 고정 시 두 번 불려도 루팅 상태를 초기화하지 않도록
  bool bCorpseStateInitialized = false;

  // 루팅 대상자(솔로=처치자, 파티=거리 내 살아 있는 파티원) 계산. 거리 내 아무도 없으면 처치자만
  void GatherRewardRecipients(ANonCharacterBase *KillerChar,
                              TArray<ANonCharacterBase *> &OutChars,
                              TArray<class ANonPlayerController *> &OutPCs,
                              bool &bOutIsParty) const;

  bool AreAllEligibleLooted() const;

  FTimerHandle CorpseRemoveTimerHandle;

  UPROPERTY(Transient)
  TArray<TObjectPtr<UMaterialInstanceDynamic>> FadeMIDs;

  bool bFadeActive = false;
  bool bIsFadingOut = false;
  float FadeStartTime = 0.f;
  float FadeLen = 0.f;

  // 페이드 중 정지시킨 이동 속도 복원용
  float SavedMaxWalkSpeed = 0.f;

  void StartCorpseTimer(float LifeTime);
  void OnCorpseExpired();
  void OnSpawnFadeFinished();

  UFUNCTION(NetMulticast, Reliable)
  void Multicast_PlayFadeOut(float Duration);

  void SetFadeValue(float Value);

  AEnemyCharacter *GetOwnerEnemy() const;
};
