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
struct FJobAbilitySet
{
    GENERATED_BODY()

    // 이 직업이 가지는 GA 목록
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Job")
    TArray<TSubclassOf<class UGameplayAbility>> Abilities;
};

UCLASS()
class NON_API ANonCharacterBase : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ANonCharacterBase();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PossessedBy(AController* NewController) override;

public:
    // ASC 캐시
    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> ASC = nullptr;

    // GAS
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    const class UNonAttributeSet* GetAttributeSet() const { return AttributeSet; }
    void InitializeAttributes();
    void GiveStartupAbilities();

    // Combo
    void RegisterCurrentComboAbility(class UGA_ComboBase* InAbility);
    void UnregisterCurrentComboAbility(class UGA_ComboBase* InAbility);

    UFUNCTION(BlueprintCallable)
    void SetComboWindowOpen(bool bOpen);
    UFUNCTION(BlueprintCallable, Category = "Combo")
    bool IsComboWindowOpen() const;

    void TryActivateCombo();
    UFUNCTION(BlueprintCallable)
    void BufferComboInput();

    UFUNCTION() void HandleAttackInput(const struct FInputActionValue& Value);
    UFUNCTION() void MoveInput(const FInputActionValue& Value);
    UFUNCTION() void LookInput(const FInputActionValue& Value);

    UFUNCTION() void LevelUp();

    UFUNCTION(BlueprintCallable, Category = "Level") void GainExp(float Amount);
    
    // [New] 강제 레벨 설정 및 스탯 갱신 (저장 로드용)
    void SetLevelAndRefreshStats(int32 NewLevel);

    // QuickSlot
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) class UQuickSlotManager* QuickSlotManager;
    UFUNCTION(BlueprintPure, Category = "QuickSlot") UQuickSlotManager* GetQuickSlotManager() const { return QuickSlotManager; }

    // === 무장 토글 ===
    UFUNCTION(BlueprintCallable, Category = "Weapon") void ToggleArmed();
    UFUNCTION(BlueprintCallable, Category = "Weapon") void SetArmed(bool bNewArmed);
    UFUNCTION(BlueprintPure, Category = "Weapon") bool IsArmed() const { return bArmed; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
    EWeaponStance WeaponStance = EWeaponStance::Unarmed;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon") EWeaponStance DefaultArmedStance = EWeaponStance::TwoHanded;
    UPROPERTY(VisibleInstanceOnly, Category = "Weapon") EWeaponStance CachedArmedStance = EWeaponStance::TwoHanded;

    UFUNCTION(BlueprintCallable, Category = "Weapon") void SetEquippedStance(EWeaponStance NewStance);
    UFUNCTION(BlueprintPure, Category = "Weapon") EWeaponStance GetEquippedStance() const { return CachedArmedStance; }

    UFUNCTION(BlueprintPure, Category = "Weapon") FName GetHandSocketForItem(const UInventoryItem* Item) const;
    UFUNCTION(BlueprintCallable, Category = "Weapon") void RefreshWeaponStance();
    UFUNCTION(BlueprintPure, Category = "Weapon") EWeaponStance GetWeaponStance() const { return WeaponStance; }

    UPROPERTY(EditDefaultsOnly, Category = "Weapon") bool bAttachViaAnimNotify = true;
    UFUNCTION(BlueprintPure, Category = "Weapon") FName GetSheathSocket1H() const { return SheathSocket1H; }
    UFUNCTION(BlueprintPure, Category = "Weapon") FName GetSheathSocket2H() const { return SheathSocket2H; }
    UFUNCTION(BlueprintPure, Category = "Weapon") FName GetHomeFallbackSocketForItem(const UInventoryItem* Item) const;

    // 이동/스트레이프
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement") bool  bStrafeMode = false;
    UFUNCTION(BlueprintCallable, Category = "Movement") void SetStrafeMode(bool bEnable);
    UFUNCTION(BlueprintPure, Category = "Movement") bool IsStrafeMode() const { return bStrafeMode; }
    UPROPERTY(EditAnywhere, Category = "Movement|Strafe") bool  bOnlyFollowCameraWhenMoving = true;
    UPROPERTY(EditAnywhere, Category = "Movement|Strafe") float YawFollowEnableSpeed = 20.f;
    UPROPERTY(VisibleAnywhere, Category = "Movement|Strafe") bool  bFollowCameraYawNow = false;

    //  회피/공격 중 회전 잠금 + 해제 후 부드러운 정렬용
    UPROPERTY(VisibleAnywhere, Category = "Movement|Strafe") bool  bRotationLockedByAbility = false;
    UPROPERTY(VisibleAnywhere, Category = "Movement|Strafe") bool  bAlignYawAfterRotationLock = false;
    UPROPERTY(EditAnywhere, Category = "Movement|Strafe") float RotationAlignInterpSpeed = 8.f;

    // 공격 시작 시 카메라 방향으로 부드럽게 정렬용
    UPROPERTY(VisibleAnywhere, Category = "Combat|AttackAlign")
    bool bAttackAlignActive = false;

    UPROPERTY(VisibleAnywhere, Category = "Combat|AttackAlign")
    FRotator AttackAlignTargetRot;

    UPROPERTY(EditAnywhere, Category = "Combat|AttackAlign")
    float AttackAlignSpeed = 15.f;   // RInterpTo 속도 (값은 나중에 튜닝)

    // 공격 시작 정렬 시작
    void StartAttackAlignToCamera();

    // 내부 업데이트용
    void UpdateAttackAlign(float DeltaSeconds);

    UPROPERTY(EditAnywhere, Category = "Movement|Speed")  float WalkSpeed_Backpedal = 150.f;
    UPROPERTY(EditAnywhere, Category = "Movement|Speed")  float BackpedalDirThresholdDeg = 135.f;
    UPROPERTY(VisibleAnywhere, Category = "Movement|Speed")  float WalkSpeed_Default = 0.f;
    void UpdateDirectionalSpeed();
    void UpdateStrafeYawFollowBySpeed();

    // === TIP (Montage Only) ===
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TIP")
    bool bEnableTIP_Armed = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TIP")
    float TIP_IdleSpeedEps = 5.f;   // Idle 판정 속도

    // 트리거 각도 / 연속발동 쿨다운
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TIP")
    float TIP_Trigger90 = 45.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TIP")
    float TIP_Trigger180 = 135.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TIP")
    float TIP_Cooldown = 0.3f;
    // "앞으로 이동 시작" 판정 파라미터
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TIP")
    float TIP_StartMoveMinSpeed = 50.f;    // 시작시 최소 속도
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TIP")
    float TIP_ForwardDotMin = 0.6f;        // 컨트롤 Forward와의 내적 임계(앞으로 입력 판단)

    // TIP 몽타주 레퍼런스
    UPROPERTY(EditAnywhere, Category = "TIP|Montage") TObjectPtr<UAnimMontage> TIP_L90 = nullptr;
    UPROPERTY(EditAnywhere, Category = "TIP|Montage") TObjectPtr<UAnimMontage> TIP_R90 = nullptr;
    UPROPERTY(EditAnywhere, Category = "TIP|Montage") TObjectPtr<UAnimMontage> TIP_L180 = nullptr;
    UPROPERTY(EditAnywhere, Category = "TIP|Montage") TObjectPtr<UAnimMontage> TIP_R180 = nullptr;

    // 상태
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TIP")
    float AimYawDelta = 0.f;

    float TIP_NextAllowedTime = 0.f;
    bool  bTIP_Pending = false;    //  카메라만 돌려서 대기 중인지
    float TIP_PendingDelta = 0.f;  //  대기 중에 저장한 각도(+는 우회전, -는 좌회전)

    bool bTIPPlaying = false;
    UFUNCTION()
    void OnTIPMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // Jump
    virtual void Jump() override;
    virtual void Landed(const FHitResult& Hit) override;

    // === Anim: Dodge/Hit 등 풀바디 강제용 플래그 ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
    bool bForceFullBody = false;

    // 몇 개의 GA가 "풀바디 필요"를 요청하고 있는지
    int32 ForceFullBodyRequestCount = 0;

    UFUNCTION(BlueprintCallable, Category = "Animation")
    void SetForceFullBody(bool bEnable);

    UFUNCTION(BlueprintPure, Category = "Animation")
    bool IsForceFullBody() const { return bForceFullBody; }

    // === Guard ===
    UFUNCTION(BlueprintPure, Category = "Guard") bool      IsGuarding() const { return bGuarding; }
    UFUNCTION(BlueprintPure, Category = "Guard") EGuardDir8 GetGuardDir8() const { return GuardDir8; }
    UFUNCTION(BlueprintCallable, Category = "Guard") void GuardPressed();
    UFUNCTION(BlueprintCallable, Category = "Guard") void GuardReleased();

    UPROPERTY(EditAnywhere, Category = "Guard|Speed") float GuardWalkSpeed = 150.f;
    UPROPERTY(EditAnywhere, Category = "Guard|Speed") float GuardBackSpeed = 150.f;
    UPROPERTY(EditAnywhere, Category = "Guard|Angle") float GuardBackEnterDeg = 135.f;
    UPROPERTY(EditAnywhere, Category = "Guard|Angle") float GuardBackExitDeg = 120.f;

    UPROPERTY(VisibleAnywhere, Category = "Guard") bool      bGuarding = false;
    UPROPERTY(VisibleAnywhere, Category = "Guard") EGuardDir8 GuardDir8 = EGuardDir8::F;
    UPROPERTY(VisibleAnywhere, Category = "Guard") float     GuardDirAngle = 0.f;

    void StartGuard();
    void StopGuard();
    void UpdateGuardDirAndSpeed();

    // 데미지/사망/히트리액트
    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void ApplyDamageAt(float Amount, AActor* DamageInstigator, const FVector& WorldLocation);
    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsDead() const;
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SpawnDamageNumber(float Amount, FVector WorldLocation, bool bIsCritical);
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SpawnDodgeText(FVector WorldLocation);

    UPROPERTY(EditDefaultsOnly, Category = "UI|Damage")
    TSubclassOf<ADamageNumberActor> DamageNumberClass;


    //skill
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    class USkillManagerComponent* SkillMgr;

    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<class USkillDataAsset> SkillDataAsset;

    // ───── Job 설정 ─────
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Job")
    EJobClass DefaultJobClass = EJobClass::Defender;

    // 공통으로 모든 직업이 가지는 GA (원하면 비워둬도 됨)
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Abilities")
    TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

    // 직업별 GA 세트 (직업별 Combo/Dodge/Skill 등)
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Abilities")
    TMap<EJobClass, FJobAbilitySet> JobAbilitySets;

    //SP Regen 
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

    /** 스킬 계수(예: 1.5 = 150%) - AnimNotify에서 AOE에 전달용 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
    float LastSkillDamageScale = 1.f;

    UFUNCTION(BlueprintCallable, Category = "Skill")
    void SetLastSkillDamageScale(float InScale)
    {
        LastSkillDamageScale = InScale;

        UE_LOG(LogTemp, Warning,
            TEXT("[Char] %s SetLastSkillDamageScale=%.2f"),
            *GetName(),
            LastSkillDamageScale);
    }

    UFUNCTION(BlueprintPure, Category = "Skill")
    float GetLastSkillDamageScale() const
    {
        return LastSkillDamageScale;
    }

protected:
    // 카메라
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera") USpringArmComponent* CameraBoom;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera") UCameraComponent* FollowCamera;

    // UI
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI") UNonUIManagerComponent* UIManager;

    // GAS
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS") TObjectPtr<UNonAbilitySystemComponent> AbilitySystemComponent;
    UPROPERTY() TObjectPtr<const UNonAttributeSet> AttributeSet;

    // 데미지/초기화/어빌리티
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Damage") TSubclassOf<UGameplayEffect> GE_Damage;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combo") bool bComboWindowOpen = false;
    UPROPERTY() TWeakObjectPtr<class UGA_ComboBase> CurrentComboAbility;
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Init")      TSubclassOf<UGameplayEffect> DefaultAttributeEffect;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level") UDataTable* LevelDataTable;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level") TSubclassOf<UGameplayEffect> GE_LevelUp;

    // 무장
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon") bool bArmed = false;

    UFUNCTION(BlueprintCallable, Category = "Weapon") void SyncEquippedStanceFromEquipment();
    UFUNCTION(BlueprintPure, Category = "Weapon") EWeaponStance ComputeStanceFromItem(const UInventoryItem* Item) const;

    UPROPERTY() TObjectPtr<UEquipmentComponent> EquipmentComp = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Sockets") FName HandSocket1H = FName("hand_r_socket");
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Sockets") FName SheathSocket1H = FName("spine_01_socket");
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Sockets") FName HandSocket2H = FName("hand_r_socket");
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Sockets") FName SheathSocket2H = FName("spine_05_socket");

    void UpdateGuardDir(const FVector2D& Input2D);
    void ApplyHitStopSelf(float Scale, float Duration);
    void HandleDeath();
    bool HasZeroHP() const;
    void OnGotHit(float Damage, AActor* InstigatorActor, const FVector& ImpactPoint);
    FVector      ComputeKnockbackDir(AActor* InstigatorActor, const FVector& ImpactPoint) const;
    EHitQuadrant ComputeHitQuadrant(const FVector& ImpactPoint, AActor* InstigatorActor = nullptr) const;


    // Death/HitReact
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Death") bool bUseDeathMontage = true;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Death", meta = (DeprecatedProperty, EditCondition = "bUseDeathMontage", DisplayName = "(Deprecated) DeathMontage"))
    TObjectPtr<UAnimMontage> DeathMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact") EKnockbackMode KnockbackMode = EKnockbackMode::Launch;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact", meta = (EditCondition = "KnockbackMode==EKnockbackMode::Launch"))
    float LaunchStrength = 500.f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact", meta = (EditCondition = "KnockbackMode==EKnockbackMode::Launch"))
    float LaunchUpward = 0.f;

    bool bDied = false;
    bool bCanHitReact = true;

    FTimerHandle HitReactCDTimer;
    FTimerHandle HitStopTimer;

    void PlayHitReact(EHitQuadrant Quad);

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