#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSetTypes.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "Combat/CombatTypes.h"
#include "NonCharacterBase.generated.h"

class UBPC_UIManager;
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


UENUM(BlueprintType)
enum class EGuardDir8 : uint8
{
    F, FR, R, BR, B, BL, L, FL
};

// 8방향
UENUM(BlueprintType)
enum class EEightDir : uint8
{
    F, FR, R, BR, B, BL, L, FL
};

class UAnimMontage;

UCLASS()
class NON_API ANonCharacterBase : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ANonCharacterBase();

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;

public:

    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> ASC = nullptr;

    // GAS
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    void InitializeAttributes();
    void GiveStartupAbilities();

    // 현재 활성 콤보 GA 인스턴스 등록/해제
    void RegisterCurrentComboAbility(class UGA_ComboBase* InAbility);
    void UnregisterCurrentComboAbility(class UGA_ComboBase* InAbility);

    // Combo
    UFUNCTION(BlueprintCallable)
    void SetComboWindowOpen(bool bOpen);
    UFUNCTION(BlueprintCallable, Category = "Combo")
    bool IsComboWindowOpen() const;
    void TryActivateCombo();
    UFUNCTION(BlueprintCallable)
    void BufferComboInput();

    UFUNCTION()
    void HandleAttackInput(const struct FInputActionValue& Value);

    UFUNCTION()
    void MoveInput(const FInputActionValue& Value);

    UFUNCTION()
    void LookInput(const FInputActionValue& Value);

    UFUNCTION()
    void LevelUp();

    UFUNCTION(BlueprintCallable, Category = "Level")
    void GainExp(float Amount);

    // QuickSlot
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    class UQuickSlotManager* QuickSlotManager;

    UFUNCTION(BlueprintPure, Category = "QuickSlot")
    UQuickSlotManager* GetQuickSlotManager() const { return QuickSlotManager; }

    // === 무장 토글 ===
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void ToggleArmed();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SetArmed(bool bNewArmed);

    UFUNCTION(BlueprintPure, Category = "Weapon")
    bool IsArmed() const { return bArmed; }

    // 현재 무기 자세(AnimBP에서 읽어감)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
    EWeaponStance WeaponStance = EWeaponStance::Unarmed;

    // ▶▶ 장착 무기 스탠스 캐시 (NEW)
    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    EWeaponStance DefaultArmedStance = EWeaponStance::TwoHanded;

    UPROPERTY(VisibleInstanceOnly, Category = "Weapon")
    EWeaponStance CachedArmedStance = EWeaponStance::TwoHanded;

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SetEquippedStance(EWeaponStance NewStance);

    UFUNCTION(BlueprintPure, Category = "Weapon")
    EWeaponStance GetEquippedStance() const { return CachedArmedStance; }

    // 무기에 맞는 "손" 소켓
    UFUNCTION(BlueprintPure, Category = "Weapon")
    FName GetHandSocketForItem(const UInventoryItem* Item) const;

    // 현재 장착 무기 기준 스탠스 갱신
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void RefreshWeaponStance();

    UFUNCTION(BlueprintPure, Category = "Weapon")
    EWeaponStance GetWeaponStance() const { return WeaponStance; }

    // 애님 노티파이로 부착 제어
    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    bool bAttachViaAnimNotify = true;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    FName GetSheathSocket1H() const { return SheathSocket1H; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    FName GetSheathSocket2H() const { return SheathSocket2H; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    FName GetHomeFallbackSocketForItem(const UInventoryItem* Item) const;

    // Draw/Sheathe (Deprecated: AnimBP DataAssets 사용)
    UPROPERTY(EditAnywhere, Category = "Weapon|Montage", meta = (DeprecatedProperty, DisplayName = "(Deprecated) Draw1H"))
    TObjectPtr<UAnimMontage> Draw1H = nullptr;
    UPROPERTY(EditAnywhere, Category = "Weapon|Montage", meta = (DeprecatedProperty, DisplayName = "(Deprecated) Sheathe1H"))
    TObjectPtr<UAnimMontage> Sheathe1H = nullptr;
    UPROPERTY(EditAnywhere, Category = "Weapon|Montage", meta = (DeprecatedProperty, DisplayName = "(Deprecated) Draw2H"))
    TObjectPtr<UAnimMontage> Draw2H = nullptr;
    UPROPERTY(EditAnywhere, Category = "Weapon|Montage", meta = (DeprecatedProperty, DisplayName = "(Deprecated) Sheathe2H"))
    TObjectPtr<UAnimMontage> Sheathe2H = nullptr;

    // FullBody 버전도 Deprecated
    UPROPERTY(EditAnywhere, Category = "Weapon|Montage", meta = (DeprecatedProperty, DisplayName = "(Deprecated) Draw1H_Full"))
    TObjectPtr<UAnimMontage> Draw1H_Full = nullptr;
    UPROPERTY(EditAnywhere, Category = "Weapon|Montage", meta = (DeprecatedProperty, DisplayName = "(Deprecated) Sheathe1H_Full"))
    TObjectPtr<UAnimMontage> Sheathe1H_Full = nullptr;
    UPROPERTY(EditAnywhere, Category = "Weapon|Montage", meta = (DeprecatedProperty, DisplayName = "(Deprecated) Draw2H_Full"))
    TObjectPtr<UAnimMontage> Draw2H_Full = nullptr;
    UPROPERTY(EditAnywhere, Category = "Weapon|Montage", meta = (DeprecatedProperty, DisplayName = "(Deprecated) Sheathe2H_Full"))
    TObjectPtr<UAnimMontage> Sheathe2H_Full = nullptr;

    UPROPERTY(EditAnywhere, Category = "Weapon|Montage")
    float FullBodyIdleSpeedThreshold = 5.f;

    UAnimMontage* PickDrawSheatheMontage(bool bDraw, bool bTwoHand, bool bUseFull) const;

    // 이동/스트레이프
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    bool bStrafeMode = false;

    UFUNCTION(BlueprintCallable, Category = "Movement")
    void SetStrafeMode(bool bEnable);

    UFUNCTION(BlueprintPure, Category = "Movement")
    bool IsStrafeMode() const { return bStrafeMode; }

    UPROPERTY(EditAnywhere, Category = "Movement|Strafe")
    bool bOnlyFollowCameraWhenMoving = true;

    UPROPERTY(EditAnywhere, Category = "Movement|Strafe")
    float YawFollowEnableSpeed = 20.f;

    UPROPERTY(VisibleAnywhere, Category = "Movement|Strafe")
    bool bFollowCameraYawNow = false;

    UPROPERTY(EditAnywhere, Category = "Movement|Speed")
    float WalkSpeed_Backpedal = 150.f;

    UPROPERTY(EditAnywhere, Category = "Movement|Speed")
    float BackpedalDirThresholdDeg = 135.f;

    UPROPERTY(VisibleAnywhere, Category = "Movement|Speed")
    float WalkSpeed_Default = 0.f;

    void UpdateDirectionalSpeed();

    // === Guard ===
    UFUNCTION(BlueprintPure, Category = "Guard")
    bool IsGuarding() const { return bGuarding; }

    UFUNCTION(BlueprintPure, Category = "Guard")
    EGuardDir8 GetGuardDir8() const { return GuardDir8; }

    UFUNCTION(BlueprintCallable, Category = "Guard")
    void GuardPressed();

    UFUNCTION(BlueprintCallable, Category = "Guard")
    void GuardReleased();

    UPROPERTY(EditAnywhere, Category = "Guard|Speed")
    float GuardWalkSpeed = 150.f;

    UPROPERTY(EditAnywhere, Category = "Guard|Speed")
    float GuardBackSpeed = 150.f;

    UPROPERTY(EditAnywhere, Category = "Guard|Angle")
    float GuardBackEnterDeg = 135.f;
    UPROPERTY(EditAnywhere, Category = "Guard|Angle")
    float GuardBackExitDeg = 120.f;

    UPROPERTY(VisibleAnywhere, Category = "Guard")
    bool bGuarding = false;

    UPROPERTY(VisibleAnywhere, Category = "Guard")
    EGuardDir8 GuardDir8 = EGuardDir8::F;

    UPROPERTY(VisibleAnywhere, Category = "Guard")
    float GuardDirAngle = 0.f;

    void StartGuard();
    void StopGuard();
    void UpdateGuardDirAndSpeed();

    // === Dodge ===
    UFUNCTION(BlueprintCallable, Category = "Dodge")
    void RequestDodge2D(const FVector2D& Input2D);

    // 스탠스별 8방향 회피 몽타주
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Unarmed") UAnimMontage* Dodge_Unarmed_F = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Unarmed") UAnimMontage* Dodge_Unarmed_FR = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Unarmed") UAnimMontage* Dodge_Unarmed_R = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Unarmed") UAnimMontage* Dodge_Unarmed_BR = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Unarmed") UAnimMontage* Dodge_Unarmed_B = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Unarmed") UAnimMontage* Dodge_Unarmed_BL = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Unarmed") UAnimMontage* Dodge_Unarmed_L = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Unarmed") UAnimMontage* Dodge_Unarmed_FL = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Dodge|1H") UAnimMontage* Dodge_1H_F = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|1H") UAnimMontage* Dodge_1H_FR = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|1H") UAnimMontage* Dodge_1H_R = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|1H") UAnimMontage* Dodge_1H_BR = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|1H") UAnimMontage* Dodge_1H_B = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|1H") UAnimMontage* Dodge_1H_BL = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|1H") UAnimMontage* Dodge_1H_L = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|1H") UAnimMontage* Dodge_1H_FL = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Dodge|2H") UAnimMontage* Dodge_2H_F = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|2H") UAnimMontage* Dodge_2H_FR = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|2H") UAnimMontage* Dodge_2H_R = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|2H") UAnimMontage* Dodge_2H_BR = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|2H") UAnimMontage* Dodge_2H_B = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|2H") UAnimMontage* Dodge_2H_BL = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|2H") UAnimMontage* Dodge_2H_L = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|2H") UAnimMontage* Dodge_2H_FL = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Staff") UAnimMontage* Dodge_Staff_F = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Staff") UAnimMontage* Dodge_Staff_FR = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Staff") UAnimMontage* Dodge_Staff_R = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Staff") UAnimMontage* Dodge_Staff_BR = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Staff") UAnimMontage* Dodge_Staff_B = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Staff") UAnimMontage* Dodge_Staff_BL = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Staff") UAnimMontage* Dodge_Staff_L = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Dodge|Staff") UAnimMontage* Dodge_Staff_FL = nullptr;


    // HitReact 방향별 (Deprecated: AnimBP Common.HitReact.Generic로 대체)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitReact", meta = (DeprecatedProperty, DisplayName = "(Deprecated) HitReact_F"))
    TObjectPtr<UAnimMontage> HitReact_F;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitReact", meta = (DeprecatedProperty, DisplayName = "(Deprecated) HitReact_B"))
    TObjectPtr<UAnimMontage> HitReact_B;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitReact", meta = (DeprecatedProperty, DisplayName = "(Deprecated) HitReact_L"))
    TObjectPtr<UAnimMontage> HitReact_L;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|HitReact", meta = (DeprecatedProperty, DisplayName = "(Deprecated) HitReact_R"))
    TObjectPtr<UAnimMontage> HitReact_R;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact")
    FName HitReactSlotName = TEXT("DefaultSlot");

    // ------------ Stamina ------------
    UPROPERTY(EditDefaultsOnly, Category = "Stamina|Dodge")
    float DodgeStaminaCost = 20.f;

    UPROPERTY(EditDefaultsOnly, Category = "Stamina|Dodge")
    float MinStaminaToDodge = 5.f;

    UPROPERTY(EditDefaultsOnly, Category = "Stamina|Dodge")
    TSubclassOf<UGameplayEffect> GE_StaminaDeltaInstant;

    // 스태미나 사용후 몇초뒤 회복
    UPROPERTY(EditDefaultsOnly, Category = "Stamina|Regen")
    float StaminaRegenDelayAfterUse = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Stamina|Regen")
    float StaminaRegenPerSecond = 10.f;

    // 몇 초마다 회복할지
    UPROPERTY(EditDefaultsOnly, Category = "Stamina|Regen")
    float StaminaRegenTickInterval = 3.0f;
    // 한번에 얼마나 회복할지
    UPROPERTY(EditDefaultsOnly, Category = "Stamina|Regen")
    float StaminaRegenTickAmount = 25.0f;

    // 받는 데미지
    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void ApplyDamageAt(float Amount, AActor* DamageInstigator, const FVector& WorldLocation);

    // ---- Death / Hit React ----
    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsDead() const;

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SpawnDamageNumber(float Amount, FVector WorldLocation, bool bIsCritical);

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SpawnDodgeText(FVector WorldLocation);

    UPROPERTY(EditDefaultsOnly, Category = "UI|Damage")
    TSubclassOf<ADamageNumberActor> DamageNumberClass;


protected:
    virtual void Tick(float DeltaSeconds) override;
    void UpdateStrafeYawFollowBySpeed();

    // 카메라
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* FollowCamera;

    // UI
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    UBPC_UIManager* UIManager;

    // GAS
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UNonAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY()
    TObjectPtr<const UNonAttributeSet> AttributeSet;

    // 플레이어가 맞을 때 사용할 데미지 GE(Instant, SetByCaller(Data.Damage))
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Damage")
    TSubclassOf<UGameplayEffect> GE_Damage;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combo")
    bool bComboWindowOpen = false;

    UPROPERTY()
    TWeakObjectPtr<class UGA_ComboBase> CurrentComboAbility;

    UPROPERTY(EditDefaultsOnly, Category = "GAS|Init")
    TSubclassOf<UGameplayEffect> DefaultAttributeEffect;

    UPROPERTY(EditDefaultsOnly, Category = "GAS|Abilities")
    TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
    UDataTable* LevelDataTable;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
    TSubclassOf<UGameplayEffect> GE_LevelUp;

    // 무장
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    bool bArmed = false;

    // 장착 무기에서 장착 스탠스 동기화 (메인 무기 기준)
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SyncEquippedStanceFromEquipment();

    // 아이템에서 스탠스를 계산 (1H/2H/Staff)
    UFUNCTION(BlueprintPure, Category = "Weapon")
    EWeaponStance ComputeStanceFromItem(const UInventoryItem* Item) const;

    UPROPERTY()
    TObjectPtr<UEquipmentComponent> EquipmentComp = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Sockets")
    FName HandSocket1H = FName("hand_r_socket");
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Sockets")
    FName SheathSocket1H = FName("spine_01_socket");
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Sockets")
    FName HandSocket2H = FName("hand_r_socket");
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Sockets")
    FName SheathSocket2H = FName("spine_05_socket");

    // Dodge 헬퍼
    bool CanDodge() const;
    EEightDir ComputeEightDirFromInput(const FVector2D& Input2D) const;
    UAnimMontage* PickDodgeMontage(EEightDir Dir) const;
    void PlayDodgeMontage(UAnimMontage* Montage) const;
    void PreDodgeCancelOptions() const;

    // Stamina 헬퍼
    bool HasEnoughStamina(float Cost) const;
    bool ConsumeStamina(float Amount);
    void ApplyStaminaDelta_Direct(float Delta);
    void KickStaminaRegenDelay();

    // 회복 루프 on/off & 1틱 처리
    void StartStaminaRegenLoop();
    void StopStaminaRegenLoop();
    void StaminaRegenTick();

    // 기존: 회복 지연 타이머 (사용 후 일정 시간 회복 중지)
    FTimerHandle StaminaRegenTimer;       // ← 기존 지연용
    // 새로 추가: 고정 간격 회복 루프 타이머
    FTimerHandle StaminaRegenLoopTimer;

    bool bAllowStaminaRegen = true;       // 지연 중엔 false

    void OnGotHit(float Damage, AActor* InstigatorActor, const FVector& ImpactPoint); // 피격 리액션(히트스톱+넉백)

    void ApplyHitStopSelf(float Scale, float Duration); // 짧은 히트스톱

    void HandleDeath();  // HP 0 → 사망 처리

    bool HasZeroHP() const;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Death")
    bool bUseDeathMontage = true;

    // Death (Deprecated: AnimBP Common.DeathMontage로 대체)
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Death", meta = (DeprecatedProperty, EditCondition = "bUseDeathMontage", DisplayName = "(Deprecated) DeathMontage"))
    TObjectPtr<UAnimMontage> DeathMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact")
    EKnockbackMode KnockbackMode = EKnockbackMode::Launch;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact", meta = (EditCondition = "KnockbackMode==EKnockbackMode::Launch"))
    float LaunchStrength = 500.f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact", meta = (EditCondition = "KnockbackMode==EKnockbackMode::Launch"))
    float LaunchUpward = 0.f;

    bool bDied = false;
    bool bCanHitReact = true;

    FTimerHandle HitReactCDTimer;
    FTimerHandle HitStopTimer;
    FVector ComputeKnockbackDir(AActor* InstigatorActor, const FVector& ImpactPoint) const;

    void PlayHitReact(EHitQuadrant Quad);
    EHitQuadrant ComputeHitQuadrant(const FVector& ImpactPoint, AActor* InstigatorActor = nullptr) const;
};
