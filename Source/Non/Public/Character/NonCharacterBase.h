#pragma once

#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSetTypes.h"
#include "Combat/CombatTypes.h"
#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Skill/SkillTypes.h"
#include "NonCharacterBase.generated.h"

class UNonUIManagerComponent;
class UAbilitySystemComponent;
class UNonAbilitySystemComponent;
class UNonAttributeSet;
class UGameplayAbility;
class UGameplayEffect;
class USpringArmComponent;
class UCameraComponent;
class UQuickSlotManager;
class UEquipmentComponent;
class UInventoryItem;
class ADamageNumberActor;
class USkillManagerComponent;

// 8방향 (캐릭터 내부 전용 열거 — 기존 그대로 유지)
UENUM(BlueprintType)
enum class EGuardDir8 : uint8 { F, FR, R, BR, B, BL, L, FL };

USTRUCT(BlueprintType)
struct FJobAbilitySet {
  GENERATED_BODY()

  // 이 직업이 가지는 GA 목록
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Job")
  TArray<TSubclassOf<class UGameplayAbility>> Abilities;
};

// [New] 시작 아이템 목록 래퍼 (TMap Value용)
USTRUCT(BlueprintType)
struct FStartingItemSet {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadOnly)
  TArray<TSubclassOf<class UInventoryItem>> Items;
};

// [New] 시작 아이템 목록 래퍼 (TMap Value용)
USTRUCT(BlueprintType)
struct NON_API FLevelRequirements {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
  int32 RequiredLevel;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
  float RequiredExp;
};

// 피격 몽타주를 무기 스탠스별로 저장하기 위한 Wrapper 구조체
USTRUCT(BlueprintType)
struct NON_API FHitReactionStanceMap {
  GENERATED_BODY()

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
  TMap<FGameplayTag, class UAnimMontage*> Montages;
};

UCLASS()
class NON_API ANonCharacterBase : public ACharacter,
                                  public IAbilitySystemInterface {
  GENERATED_BODY()

public:
  ANonCharacterBase();

protected:
  virtual void BeginPlay() override;
  virtual void
  EndPlay(const EEndPlayReason::Type EndPlayReason) override; // [New]
  virtual void Tick(float DeltaSeconds) override;
  virtual void PossessedBy(AController *NewController) override;

  // [Multiplayer] 리플리케이션 속성 정의
  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;

public:
  // --- [Modular Character Parts] ---
  // 메인 Mesh(GetMesh)를 Master로 쓰고, 아래 부위들이 따라움직임(Master Pose)

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Parts")
  TObjectPtr<USkeletalMeshComponent> HeadMesh;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Parts")
  TObjectPtr<USkeletalMeshComponent> HairMesh;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Parts")
  TObjectPtr<USkeletalMeshComponent> EyebrowsMesh;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Parts")
  TObjectPtr<USkeletalMeshComponent> LegsMesh; // 하의 (Pants)

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Parts")
  TObjectPtr<USkeletalMeshComponent> HandsMesh; // 장갑 (Gloves)

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Parts")
  TObjectPtr<USkeletalMeshComponent> FeetMesh; // 신발 (Boots)
  // ASC 캐시
  UPROPERTY()
  TObjectPtr<UAbilitySystemComponent> ASC = nullptr;

  // GAS
  virtual UAbilitySystemComponent *GetAbilitySystemComponent() const override;
  const class UNonAttributeSet *GetAttributeSet() const { return AttributeSet; }
  void InitializeAttributes();
  void GiveStartupAbilities();

  // Combo
  void RegisterCurrentComboAbility(class UGA_ComboBase *InAbility);
  void UnregisterCurrentComboAbility(class UGA_ComboBase *InAbility);

  UFUNCTION(BlueprintCallable)
  void SetComboWindowOpen(bool bOpen);
  UFUNCTION(BlueprintCallable, Category = "Combo")
  bool IsComboWindowOpen() const;

  void TryActivateCombo();
  UFUNCTION(BlueprintCallable)
  void BufferComboInput();

  virtual void SetupPlayerInputComponent(
      class UInputComponent *PlayerInputComponent) override;

  // --- Enhanced Input ---
  // [Refactor] Controller로 이동됨 (필요하다면 다시 살려서 독립적으로 쓸 수도
  // 있음)
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
  class UInputMappingContext *DefaultMappingContext;

  /* Controller로 이관되어 주석 처리 (컴파일 에러 방지용으로 남겨둬도 되지만
  깔끔하게 정리) UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category =
  "Input") class UInputAction* MoveAction;
  ...
  */

  UFUNCTION() void HandleAttackInput(const struct FInputActionValue &Value);
  UFUNCTION() void MoveInput(const FInputActionValue &Value);
  UFUNCTION() void LookInput(const FInputActionValue &Value);

  UFUNCTION() void LevelUp();

  UFUNCTION(BlueprintCallable, Category = "Level") void GainExp(float Amount);

  // [New] 강제 레벨 설정 및 스탯 갱신 (저장 로드용)
  void SetLevelAndRefreshStats(int32 NewLevel);
  // [New] 캐릭터 전체 초기화 (직업 변경 + 능력치 재설정)
  void InitCharacterData(EJobClass NewJob, int32 NewLevel);

  // QuickSlot
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
  class UQuickSlotManager *QuickSlotManager;
  UFUNCTION(BlueprintPure, Category = "QuickSlot")
  UQuickSlotManager *GetQuickSlotManager() const { return QuickSlotManager; }

  // === 타겟팅 (Target Frame) ===
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat")
  TObjectPtr<class AEnemyCharacter> CurrentTarget;

  // [New] 타겟 변경 빈도 제한용 시간
  float LastTargetSetTime = 0.f;

  UFUNCTION(BlueprintCallable, Category = "Combat")
  void SetCombatTarget(class AEnemyCharacter *NewTarget);

  // === 무장 토글 ===
  UFUNCTION(BlueprintCallable, Category = "Combat")
  void ToggleArmed();
  UFUNCTION(BlueprintCallable, Category = "Weapon")
  void SetArmed(bool bNewArmed);
  UFUNCTION(BlueprintPure, Category = "Weapon") bool IsArmed() const {
    return bArmed;
  }

  // Camera Zoom
  UFUNCTION(BlueprintCallable, Category = "Camera")
  void CameraZoom(float Value);

  // [Multiplayer] 디버그용 아이템 추가 RPC
  UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Debug")
  void ServerAddItem(FName ItemId, int32 Quantity);

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
  float CameraZoomMin = 200.f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
  float CameraZoomMax = 500.f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
  float CameraZoomStep = 50.f;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon",
            meta = (AllowPrivateAccess = "true"))
  EWeaponStance WeaponStance = EWeaponStance::Unarmed;

  UPROPERTY(EditDefaultsOnly, Category = "Weapon")
  EWeaponStance DefaultArmedStance = EWeaponStance::Unarmed;
  UPROPERTY(VisibleInstanceOnly, Category = "Weapon")
  EWeaponStance CachedArmedStance = EWeaponStance::Unarmed;

  UFUNCTION(BlueprintCallable, Category = "Weapon")
  void SetEquippedStance(EWeaponStance NewStance);
  UFUNCTION(BlueprintPure, Category = "Weapon")
  EWeaponStance GetEquippedStance() const { return CachedArmedStance; }

  UFUNCTION(BlueprintPure, Category = "Weapon")
  FName GetHandSocketForItem(const UInventoryItem *Item) const;
  UFUNCTION(BlueprintCallable, Category = "Weapon") void RefreshWeaponStance();
  UFUNCTION(BlueprintPure, Category = "Weapon")
  EWeaponStance GetWeaponStance() const { return WeaponStance; }

  UPROPERTY(EditDefaultsOnly, Category = "Weapon")
  bool bAttachViaAnimNotify = true;
  UFUNCTION(BlueprintPure, Category = "Weapon")
  FName GetSheathSocket1H() const { return SheathSocket1H; }
  UFUNCTION(BlueprintPure, Category = "Weapon")
  FName GetSheathSocket2H() const { return SheathSocket2H; }
  UFUNCTION(BlueprintPure, Category = "Weapon")
  FName GetHomeFallbackSocketForItem(const UInventoryItem *Item) const;

  // 이동/스트레이프
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
            ReplicatedUsing = OnRep_StrafeMode, Category = "Movement")
  bool bStrafeMode = false;

  UFUNCTION()
  void OnRep_StrafeMode();

  UFUNCTION(BlueprintCallable, Category = "Movement")
  void SetStrafeMode(bool bEnable);
  UFUNCTION(BlueprintPure, Category = "Movement") bool IsStrafeMode() const {
    return bStrafeMode;
  }
  UPROPERTY(EditAnywhere, Category = "Movement|Strafe")
  bool bOnlyFollowCameraWhenMoving = true;
  UPROPERTY(EditAnywhere, Category = "Movement|Strafe")
  float YawFollowEnableSpeed = 20.f;
  UPROPERTY(VisibleAnywhere, Category = "Movement|Strafe")
  bool bFollowCameraYawNow = false;

  //  회피/공격 중 회전 잠금 + 해제 후 부드러운 정렬용
  UPROPERTY(VisibleAnywhere, Category = "Movement|Strafe")
  bool bRotationLockedByAbility = false;
  UPROPERTY(VisibleAnywhere, Category = "Movement|Strafe")
  bool bAlignYawAfterRotationLock = false;
  UPROPERTY(EditAnywhere, Category = "Movement|Strafe")
  float RotationAlignInterpSpeed = 8.f;

  // 공격 시작 시 카메라 방향으로 부드럽게 정렬용
  UPROPERTY(VisibleAnywhere, Category = "Combat|AttackAlign")
  bool bAttackAlignActive = false;

  UPROPERTY(VisibleAnywhere, Category = "Combat|AttackAlign")
  FRotator AttackAlignTargetRot;

  UPROPERTY(EditAnywhere, Category = "Combat|AttackAlign")
  float AttackAlignSpeed = 15.f; // RInterpTo 속도 (값은 나중에 튜닝)

  // 공격 시작 정렬 시작
  void StartAttackAlignToCamera();

  // 내부 업데이트용
  void UpdateAttackAlign(float DeltaSeconds);

  UPROPERTY(EditAnywhere, Category = "Movement|Speed")
  float WalkSpeed_Backpedal = 150.f;
  UPROPERTY(EditAnywhere, Category = "Movement|Speed")
  float BackpedalDirThresholdDeg = 135.f;
  UPROPERTY(VisibleAnywhere, Category = "Movement|Speed")
  float WalkSpeed_Default = 0.f;

  // [Multiplayer] Backpedal 상태 리플리케이션
  UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_IsBackpedaling,
            Category = "Movement|Speed")
  bool bIsBackpedaling = false;

  UFUNCTION()
  void OnRep_IsBackpedaling();

  UFUNCTION(Server, Reliable)
  void ServerSetBackpedaling(bool bNewBackpedaling);

  void UpdateDirectionalSpeed();
  void UpdateStrafeYawFollowBySpeed();

  // [Fix] 회피 방향 미리 계산 (Client Prediction용)
  // 0:Fwd, 1:Back, 2:Left, 3:Right, ... (enum 매핑)
  UFUNCTION(BlueprintPure, Category = "Dodge")
  int32 GetDesiredDodgeDirection() const;

  // [New] RPC 기반 회피 동기화
  int32 TempDodgeDirection = 0; // GA에서 참조

  UFUNCTION(Server, Reliable)
  void ServerActivateDodge(int32 DirIndex, float CurrentYaw);

  // [New] 적 타격 시 클라이언트 UI 알림 (체력바 표시용)
  UFUNCTION(Client, Reliable)
  void ClientOnAttackHitEnemy(class AEnemyCharacter *TargetEnemy);

  bool bAttributesInitialized = false; // [New] 중복 초기화 방지용

  // [New] 직업별 시작 아이템 래퍼 구조체 (TMap 중첩 컨테이너 지원용)
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Job|Equipment")
  TMap<EJobClass, FStartingItemSet> JobStartingItems;

  // [New] 장비 초기화 함수
  void EquipStartingItemsForJob(EJobClass Job);

  // Jump
  virtual void Jump() override;
  virtual void Landed(const FHitResult &Hit) override;

  // === Anim: Dodge/Hit 등 풀바디 강제용 플래그 ===
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated,
            Category = "Animation")
  bool bForceFullBody = false;

  // 몇 개의 GA가 "풀바디 필요"를 요청하고 있는지
  int32 ForceFullBodyRequestCount = 0;

  // [New] 조준 데칼 중 카메라 회전 차단 플래그
  bool bLookInputBlocked = false;

  UFUNCTION(BlueprintCallable, Category = "Animation")
  void SetForceFullBody(bool bEnable);

  UFUNCTION(BlueprintPure, Category = "Animation")
  bool IsForceFullBody() const { return bForceFullBody; }

  // [New] 조준 데칼 사용 중 카메라 회전 잠금
  UFUNCTION(BlueprintCallable, Category = "Input")
  void SetLookInputBlocked(bool bBlock) { bLookInputBlocked = bBlock; }

  UFUNCTION(BlueprintPure, Category = "Input")
  bool IsLookInputBlocked() const { return bLookInputBlocked; }

  // === Guard ===
  UFUNCTION(BlueprintPure, Category = "Guard") bool IsGuarding() const {
    return bGuarding;
  }
  UFUNCTION(BlueprintPure, Category = "Guard") EGuardDir8 GetGuardDir8() const {
    return GuardDir8;
  }
  UFUNCTION(BlueprintCallable, Category = "Guard") void GuardPressed();
  UFUNCTION(BlueprintCallable, Category = "Guard") void GuardReleased();

  UPROPERTY(EditAnywhere, Category = "Guard|Speed")
  float GuardWalkSpeed = 150.f;
  UPROPERTY(EditAnywhere, Category = "Guard|Speed")
  float GuardBackSpeed = 150.f;
  UPROPERTY(EditAnywhere, Category = "Guard|Angle")
  float GuardBackEnterDeg = 135.f;
  UPROPERTY(EditAnywhere, Category = "Guard|Angle")
  float GuardBackExitDeg = 120.f;

  UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_IsGuarding,
            Category = "Guard")
  bool bGuarding = false;

  UFUNCTION()
  void OnRep_IsGuarding();

  UFUNCTION(Server, Reliable)
  void ServerSetGuarding(bool bEnable);

  UPROPERTY(VisibleAnywhere, Category = "Guard")
  EGuardDir8 GuardDir8 = EGuardDir8::F;
  UPROPERTY(VisibleAnywhere, Category = "Guard") float GuardDirAngle = 0.f;

  void StartGuard();
  void StopGuard();
  void UpdateGuardDirAndSpeed();

  // 데미지/사망/히트리액트
  UFUNCTION(BlueprintCallable, Category = "Combat")
  virtual void ApplyDamageAt(float Amount, AActor *DamageInstigator,
                             const FVector &WorldLocation,
                             FGameplayTag ReactionTag = FGameplayTag());
  UFUNCTION(BlueprintPure, Category = "Combat")
  bool IsDead() const;
  UFUNCTION(NetMulticast, Unreliable)
  void Multicast_SpawnDamageNumber(float Amount, FVector WorldLocation,
                                   bool bIsCritical);
  UFUNCTION(NetMulticast, Unreliable)
  void Multicast_SpawnDodgeText(FVector WorldLocation);

  UPROPERTY(EditDefaultsOnly, Category = "UI|Damage")
  TSubclassOf<ADamageNumberActor> DamageNumberClass;

  // skill
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
  class USkillManagerComponent *SkillMgr;

  UPROPERTY(EditDefaultsOnly)
  TObjectPtr<class USkillDataAsset> SkillDataAsset;

  // [New] 플레이어 이름 (런타임 저장용)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Player")
  FString PlayerName = TEXT("Player");

  UFUNCTION(BlueprintCallable, Category = "Player")
  void SetPlayerName(const FString &NewName) { PlayerName = NewName; }

  UFUNCTION(BlueprintPure, Category = "Player")
  FString GetPlayerName() const { return PlayerName; }

  // ───── Job 설정 ─────
  UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_JobClass,
            Category = "Job")
  EJobClass DefaultJobClass = EJobClass::Defender;

  UFUNCTION()
  void OnRep_JobClass();

  // 공통으로 모든 직업이 가지는 GA (원하면 비워둬도 됨)
  UPROPERTY(EditDefaultsOnly, Category = "GAS|Abilities")
  TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

  // 직업별 GA 세트 (직업별 Combo/Dodge/Skill 등)
  UPROPERTY(EditDefaultsOnly, Category = "GAS|Abilities")
  TMap<EJobClass, FJobAbilitySet> JobAbilitySets;

  // [New] 현재 장착된 피격 리액션 어빌리티 핸들 (교체용)
  void SendJobInfoToClient(EJobClass NewJob);

  // SP Regen
  UPROPERTY(EditDefaultsOnly, Category = "GAS|Effects")
  TSubclassOf<UGameplayEffect> StaminaRegenEffectClass;

  UFUNCTION(BlueprintCallable)
  void TryDodge();

  /** 전투 상태 진입 (공격/피격/어그로 발생 시 호출) */
  UFUNCTION(BlueprintCallable, Category = "Combat")
  void EnterCombatState();

  /** 전투 상태 종료 (타이머/강제 종료용) */
  UFUNCTION(BlueprintCallable, Category = "Combat")
  void LeaveCombatState();

  UFUNCTION(Exec)
  void SaveGameTest();

  UFUNCTION(Exec)
  void ResetGameTest();

  // [New] 스킬 데미지/레벨 캐싱 (NotifyState에서 쓰기 위함)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Skill")
  float CachedSkillDamageScale = 1.0f;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Skill")
  int32 CachedSkillLevel = 1;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Skill")
  float CachedSkillStunDuration = 0.0f;

  UFUNCTION(BlueprintCallable, Category = "Combat|Skill")
  void SetLastSkillDamageScale(float InScale) {
    CachedSkillDamageScale = InScale;
  }

  UFUNCTION(BlueprintPure, Category = "Combat|Skill")
  float GetLastSkillDamageScale() const { return CachedSkillDamageScale; }

  UFUNCTION(BlueprintCallable, Category = "Combat|Skill")
  void SetLastSkillLevel(int32 InLevel) {
      CachedSkillLevel = InLevel;
  }

  UFUNCTION(BlueprintPure, Category = "Combat|Skill")
  int32 GetLastSkillLevel() const { return CachedSkillLevel; }

  UFUNCTION(BlueprintCallable, Category = "Combat|Skill")
  void SetLastSkillStunDuration(float InDuration) {
      CachedSkillStunDuration = InDuration;
  }

  UFUNCTION(BlueprintPure, Category = "Combat|Skill")
  float GetLastSkillStunDuration() const { return CachedSkillStunDuration; }

  // [New] 테스트용 강제 레벨업 (블루프린트 호출 가능)
  UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Debug")
  void ServerDebugLevelUp();

protected:
  // 카메라
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
  USpringArmComponent *CameraBoom;
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
  UCameraComponent *FollowCamera;

  // UI
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
  UNonUIManagerComponent *UIManagerComp;

  // Inventory
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
  class UInventoryComponent *InventoryComp;

  // GAS
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
  TObjectPtr<UNonAbilitySystemComponent> AbilitySystemComponent;
  UPROPERTY() TObjectPtr<const UNonAttributeSet> AttributeSet;

  // 데미지/초기화/어빌리티
  UPROPERTY(EditDefaultsOnly, Category = "GAS|Damage")
  TSubclassOf<UGameplayEffect> GE_Damage;
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combo")
  bool bComboWindowOpen = false;
  UPROPERTY() TWeakObjectPtr<class UGA_ComboBase> CurrentComboAbility;
  UPROPERTY(EditDefaultsOnly, Category = "GAS|Init")
  TSubclassOf<UGameplayEffect> DefaultAttributeEffect;
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
  UDataTable *LevelDataTable;
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
  TSubclassOf<UGameplayEffect> GE_LevelUp;

  // 무장
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
  bool bArmed = false;

  UFUNCTION(BlueprintCallable, Category = "Weapon")
  void SyncEquippedStanceFromEquipment();
  UFUNCTION(BlueprintPure, Category = "Weapon")
  EWeaponStance ComputeStanceFromItem(const UInventoryItem *Item) const;

  UPROPERTY() TObjectPtr<UEquipmentComponent> EquipmentComp = nullptr;

  UPROPERTY(EditDefaultsOnly, Category = "Weapon|Sockets")
  FName HandSocket1H = FName("hand_r_socket");
  UPROPERTY(EditDefaultsOnly, Category = "Weapon|Sockets")
  FName SheathSocket1H = FName("spine_01_socket");
  UPROPERTY(EditDefaultsOnly, Category = "Weapon|Sockets")
  FName HandSocket2H = FName("hand_r_socket");
  UPROPERTY(EditDefaultsOnly, Category = "Weapon|Sockets")
  FName SheathSocket2H = FName("spine_05_socket");

  void ApplyHitStopSelf(float Scale, float Duration);
  
public:
  // 캐릭터 사망 처리 기본 흐름
  UFUNCTION()
  virtual void HandleDeath();

  // 사망 몽타주 종료 후 자세 고정 (플레이어 전용)
  UFUNCTION()
  virtual void FreezeDeathPose();

  // 무기(스탠스)별 피격 몽타주를 매핑해 둔 저장소
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitReaction")
  TMap<EWeaponStance, FHitReactionStanceMap> StanceHitMontages;

  // 무기 스탠스와 태그에 맞는 피격 몽타주 반환
  UFUNCTION(BlueprintCallable, Category = "Combat|HitReaction")
  class UAnimMontage* GetHitMontage(FGameplayTag HitTag) const;

protected:
  bool HasZeroHP() const;
  virtual void OnGotHit(float Damage, AActor *InstigatorActor,
                        const FVector &ImpactPoint,
                        FGameplayTag ReactionTag = FGameplayTag());

  bool bDied = false;
  bool bCanHitReact = true;

  FTimerHandle HitReactCDTimer;
  FTimerHandle HitStopTimer;

  /** 전투 상태 유지 타이머 */
  FTimerHandle CombatStateTimerHandle;

  /** 전투 상태 유지 시간 (초) – 공격/피격 후 이 시간 동안 State.Combat 유지 */
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
  float CombatStateDuration = 10.f;

private:
  bool IsAnyMontagePlaying() const;

  UFUNCTION(BlueprintPure, Category = "Combat")
  bool IsInCombat() const;
};