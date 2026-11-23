#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Combat/CombatTypes.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interaction/NonInteractableInterface.h"
#include "Combat/NonDamageHelpers.h"
#include "GameplayTagContainer.h" // [Fix] Added missing include
#include "EnemyCharacter.generated.h"

class UAbilitySystemComponent;
class UNonAbilitySystemComponent;
class UNonAttributeSet;
class UGameplayEffect;
class UWidgetComponent;
class ADamageNumberActor;
class UBoxComponent;
class USphereComponent;
class UEnemyCorpseComponent;
class UEnemyDataAsset;
class UDataTable;
class ANonCharacterBase;
class UGameplayAbility; // [Fix] Forward declaration
class UMotionWarpingComponent;
class UArrowComponent;

UENUM(BlueprintType)
enum class EAggroStyle : uint8
{
    Preemptive UMETA(DisplayName = "Preemptive"), // 근접/시야 등으로 먼저 어그로
    Reactive   UMETA(DisplayName = "Reactive")   // 맞아야만 어그로
};

UENUM(BlueprintType)
enum class EEnemyCombatStyle : uint8
{
    Melee    UMETA(DisplayName = "근접 전용 (Melee)"),
    Ranged   UMETA(DisplayName = "원거리 전용 (Ranged)"),
    Hybrid   UMETA(DisplayName = "근접/원거리 하이브리드 (Hybrid)")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDied, AEnemyCharacter*, Enemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyAlerted, AActor*, TargetPlayer);

UCLASS()
class NON_API AEnemyCharacter : public ACharacter, public IAbilitySystemInterface, public INonInteractableInterface
{
    GENERATED_BODY()

public:
    AEnemyCharacter();

    // C++로 자동 생성 및 관리되는 모션 워핑 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MotionWarping")
    TObjectPtr<UMotionWarpingComponent> MotionWarpingComp;

    // 몬스터 고유 순찰 걷기 속도 (DT_EnemyData에서 자동 주입 - BP 기본값에서는 숨김)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Movement")
    float PatrolWalkSpeed = 200.0f;

    // 몬스터 고유 전투 달리기/추적 속도 (DT_EnemyData에서 자동 주입 - BP 기본값에서는 숨김)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Movement")
    float CombatRunSpeed = 400.0f;

    // 조준/선회 시 초당 회전 속도 (도/초, 기본 220.0f - DT_EnemyData에서 자동 주입)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Movement")
    float AimTurnRate = 220.0f;

public:
    // IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnEnemyDied OnEnemyDied;

    /** 선공 몬스터가 플레이어를 최초 발견했을 때 호출되는 이벤트 (블루프린트에서 느낌표 UI, 사운드, 파티클 연결 가능) */
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnEnemyAlerted OnEnemyAlerted;

    // [New] (타겟 프레임용) AttributeSet 접근자
    UFUNCTION(BlueprintCallable, Category = "GAS")
    class UNonAttributeSet* GetAttributeSet() const { return AttributeSet; }

    // [New] (타겟 프레임용) 이름 반환
    UFUNCTION(BlueprintCallable, Category = "Config")
    FString GetEnemyName() const;

    /** 타겟(플레이어)과의 상대 각도(Yaw)를 계산합니다. (-180도 ~ +180도) */
    UFUNCTION(BlueprintPure, Category = "Enemy|AI")
    float GetAngleToTarget(const AActor* TargetActor) const;

    /** 보스 몬스터 여부 반환 (DT bIsBoss 체크 또는 ABossCharacter 상속 여부) */
    UFUNCTION(BlueprintPure, Category = "Enemy|Boss")
    bool IsBoss() const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    bool bDied = false;

    FTimerHandle AlertTimerHandle;
    FTimerHandle AlertWidgetTimerHandle;
    UPROPERTY()
    TWeakObjectPtr<AActor> PendingAlertTarget = nullptr;

public:
    // 능력치 초기화(GE 적용)
    void InitializeAttributes();

    // HP바 갱신
    virtual void UpdateHPBar() const;

    //  EnemyDataAsset으로부터 값 세팅
    UFUNCTION(BlueprintCallable, Category = "Config")
    void InitFromDataAsset(const UEnemyDataAsset* InData);

    // [New] 데이터 테이블 행(FEnemyDataTableRow)으로부터 값 세팅
    void InitFromDataTableRow(const struct FEnemyDataTableRow& InRow);

    // [Updated] 데미지 숫자 표시 (가상 함수화하여 상속 가능하게 변경)
    UFUNCTION(NetMulticast, Unreliable)
    virtual void Multicast_SpawnDamageNumber(float Amount, FVector WorldLocation, bool bIsCritical);

    UFUNCTION(NetMulticast, Unreliable)
    virtual void Multicast_SpawnImmuneText(FVector WorldLocation);

    UFUNCTION(NetMulticast, Unreliable)
    virtual void Multicast_SpawnDodgeText(FVector WorldLocation);

    UPROPERTY(EditDefaultsOnly, Category = "UI|Damage")
    TSubclassOf<ADamageNumberActor> DamageNumberActorClass;

    // 데미지 적용 (양수=피해)
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ApplyDamage(float Amount, AActor* DamageInstigator);

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsDead() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void ApplyDamageAt(float Amount, AActor* DamageInstigator, const FVector& WorldLocation, bool bIsCritical = false, FGameplayTag ReactionTag = FGameplayTag());

    // ── [New] 동일 공격자 중복 피격 방지 쿨다운 맵 (0.25초) ──
    UPROPERTY(Transient)
    TMap<TWeakObjectPtr<AActor>, float> RecentAttackerHitTimes;



    /* 상위 클래스로 이동됨
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SpawnDamageNumber(float Amount, FVector WorldLocation, bool bIsCritical);

    UPROPERTY(EditDefaultsOnly, Category = "UI|Damage")
    TSubclassOf<ADamageNumberActor> DamageNumberActorClass;
    */
    
    // ───── Aggro/공격 ─────

    // ───── Aggro/공격 (DT_EnemyData에서 자동 주입 - BP 기본값에서는 숨김) ─────
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|Attack")
    EEnemyCombatStyle CombatStyle = EEnemyCombatStyle::Melee;

    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|Attack")
    float AttackRange = 220.f;

    // 원거리 공격 사정거리 (DT_EnemyData에서 자동 주입 - BP 기본값에서는 숨김)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|Attack")
    float RangeAttackRange = 1200.f;

    // 적 기본 공격 타입/계수 (DT_EnemyData에서 자동 주입 - BP 기본값에서는 숨김)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|Attack")
    ENonDamageType AttackDamageType = ENonDamageType::Physical;

    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|Attack")
    float AttackPowerScale = 1.0f;

    void SetAggro(bool bNewAggro);
    bool IsAggro() const { return bAggro; }

    // 현재 추적(어그로) 중인 대상. 서버(BTService_UpdateTarget)에서 설정, 클라이언트로 복제.
    // 클라이언트는 이 값으로 "나를 노리는 적"을 판단해 머리 위 HP바/상단 타겟 HUD를 표시.
    void SetAggroTarget(AActor* NewTarget);
    AActor* GetAggroTarget() const { return AggroTarget; }
    bool IsTargetingLocalPlayer() const;
    void TryStartAttack();

    // [Legacy Removed] PlayAttackMontage, PickAttackMontage (Moved to GA_EnemyAttack)

    // 히트박스 On/Off (AnimNotify에서 호출)
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void AttackHitbox_Enable();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void AttackHitbox_Disable();

    /** 현재 공격 히트박스가 활성화(타격 판정 중)되어 있는지 여부 */
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|Attack")
    bool bIsAttackHitboxActive = false;

    /** 공격 준비 동작(선딜레이, 히트박스 활성화 직전) 동안 플레이어를 향해 천천히 유도 회전할지 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack")
    bool bAllowAttackWindupTracking = true;

    /** 공격 선딜레이 중 플레이어를 따라 회전하는 초당 각도 (보스 기본 75도/초 - 묵직하게 따라오다가 타격 순간 완전 고정) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack", meta = (ClampMin = "0.0", ClampMax = "360.0"))
    float AttackWindupTurnRate = 75.0f;

    /** 현재 진행 중인 공격의 피격 리액션 태그 (ANS_AttackHitboxWindow에서 자동 전달) */
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|Attack")
    FGameplayTag CurrentAttackHitReactionTag;

    // ───── Hit React (Legacy Removed) ─────
    // GAS로 이관됨 (GA_HitReaction)

    // Attacks checking logic using AnimNotify is still required below
    
    // 공격 딜레이(피격 후)
    UPROPERTY(EditDefaultsOnly, Category = "Combat|AI")
    float AttackDelayAfterHit = 0.6f;   // 피격 후 N초 동안 공격 금지

    // 기본(근접) 공격 쿨타임 (DT_EnemyData에서 자동 주입)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|Cooldown")
    float AttackCooldown = 1.5f;

    // 원거리 공격 쿨타임 (DT_EnemyData에서 자동 주입)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|Cooldown")
    float RangeAttackCooldown = 4.0f;

    // 다음 근접 공격 가능 시각(TimeSeconds)
    float NextMeleeAttackAllowedTime = 0.f;

    // 다음 원거리 공격 가능 시각(TimeSeconds)
    float NextRangeAttackAllowedTime = 0.f;

    UFUNCTION(BlueprintPure, Category = "Combat|AI")
    bool IsMeleeAttackAllowed() const;

    UFUNCTION(BlueprintPure, Category = "Combat|AI")
    bool IsRangeAttackAllowed() const;

    void TriggerMeleeAttackCooldown();
    void TriggerRangeAttackCooldown();

    // 다음 공격 가능 시각(TimeSeconds) - 하위 호환
    float NextAttackAllowedTime = 0.f;

    // 사정거리 진입 후 첫 공격 뜸들이기 대기 시간 (DT_EnemyData에서 자동 주입 - BP 기본값에서는 숨김)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|AI")
    float FirstAttackWindupMin = 0.5f;

    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|AI")
    float FirstAttackWindupMax = 1.0f;

    float CurrentWindupTime = 0.f; // 이번 진입에 적용될 대기 시간

    // 내부 상태
    float EnterRangeTime = -1.f;

    // 진입 시각 마킹/검사
    void MarkEnteredAttackRange();
    bool IsFirstAttackWindupDone() const;

    UFUNCTION(BlueprintCallable, Category = "Combat|AI")
    void BlockAttackFor(float Seconds);

    UFUNCTION(BlueprintPure, Category = "Combat|AI")
    bool IsAttackAllowed() const;
    
    // [Legacy Hit Move Pause Removed] - GA_HitReaction에서 처리 권장

    // 사망 몽타주 (보스는 BP 디테일 창에서 직접 등록, 일반 몬스터는 DT_EnemyData에서 자동 주입)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    TObjectPtr<UAnimMontage> DeathMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact")
    FName HitReactSlotName = TEXT("DefaultSlot");

    // 🔄 제자리 선회 회전 몽타주 (좌/우) - GA_EnemyTurn이 캐릭터에서 직접 읽어옵니다!
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Turn")
    TObjectPtr<UAnimMontage> TurnLeftMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Turn")
    TObjectPtr<UAnimMontage> TurnRightMontage = nullptr;



    // ---- 데이터 테이블 기반 설정 (DT_EnemyData와 100% 동기화) ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    UDataTable* EnemyDataTable = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Config", meta = (GetOptions = "GetEnemyRowNames"))
    FName EnemyRowName;

    /** 커스텀 비헤이비어 트리 (설정 시 AIController의 기본 BT 대신 이 트리를 우선 실행합니다) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    TObjectPtr<class UBehaviorTree> CustomBehaviorTree = nullptr;

    /** 보스 몬스터 여부 (DT_EnemyData에서 자동 동기화) */
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Boss")
    bool bIsBoss = false;

    /** 화면 상단 HUD 표시용 몬스터 이름 (DT_EnemyData에서 자동 동기화되며 네트워크 복제됨) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Info")
    FText EnemyDisplayName;

    /** 선공(Preemptive) 최초 발견 시 재생할 경계/포효 애니메이션 몽타주 (선택 사항) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Alert")
    TObjectPtr<class UAnimMontage> AlertMontage = nullptr;

    /** 선공(Preemptive) 최초 발견 시 머리 위 경계 효과(느낌표/효과선 위젯)를 띄울지 여부 (DT_EnemyData에서 자동 동기화) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Alert")
    bool bShowAlertMark = true;

    /** 선공(Preemptive) 최초 발견 시 플레이어를 주시하며 멈칫거리는 경계 텀(대기 시간, 초 단위, 기본 0.8초) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Alert", meta = (ClampMin = "0.0"))
    float AlertDelay = 0.8f;

    /** 현재 최초 경계 반응(텀) 진행 중인지 여부 */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Config|Alert")
    bool bIsAlerting = false;

    /** 마지막으로 전투 상호작용(내가 공격하거나 플레이어에게 피격당한 시각) */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat")
    float LastCombatActionTime = -1.0f;

    /** 전투 상호작용 시각을 현재 시각으로 갱신 */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void UpdateCombatActionTime();

    /** 마지막으로 경계(!) 반응을 실행한 시각 (단시간 내 무한 반복 경계 방지) */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Alert")
    float LastAlertTime = -100.0f;

    /** 최초 타겟 발견 경계 반응 트리거 (플레이어를 향해 회전, 몽타주/텀 시작, 이벤트 방송) */
    UFUNCTION(BlueprintCallable, Category = "Enemy|Alert")
    void TriggerAlertReaction(AActor* InTarget);

    /** 경계 텀 종료 후 정상 추적 전환 */
    UFUNCTION()
    void FinishAlertReaction();

    /** 네트워크(클라이언트/서버 공통) 머리 위 경계 마크 표시 */
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ShowAlertMark(float ShowDuration);

    /** 네트워크(클라이언트/서버 공통) 머리 위 경계 마크 숨김 */
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_HideAlertMark();

    UFUNCTION()
    TArray<FString> GetEnemyRowNames() const;

    // ---- EnemyData (런타임에 C++가 자동 주입 - BP 기본값에서는 숨김) ----
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, ReplicatedUsing = OnRep_EnemyData, Category = "Config")
    const UEnemyDataAsset* EnemyData = nullptr;

    UFUNCTION()
    void OnRep_EnemyData();

    // ---- 실시간 런타임 스탯 (DT_EnemyData에서 자동 주입 - BP 기본값에서는 숨김) ----
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Stats")
    float BaseAttack = 10.f;

    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Stats")
    float BaseDefense = 5.f;

    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Reward")
    float ExpReward = 10.f;

    // 파티 경험치 분배 설정(반경/보너스)은 CorpseComponent로 이동
    void DistributeExpOnDeath(ANonCharacterBase* KillerChar);

    // 등장/소멸 페이드, 시체 루팅, 경험치 분배 담당
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Corpse")
    TObjectPtr<class UEnemyCorpseComponent> CorpseComponent;

    UFUNCTION(BlueprintPure, Category = "Corpse")
    UEnemyCorpseComponent* GetCorpseComponent() const { return CorpseComponent; }

    USphereComponent* GetInteractCollision() const { return InteractCollision; }

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI")
    FVector SpawnLocation = FVector::ZeroVector;

    /** 홈 리쉬(스폰 지점 복귀) 진행 중 플래그 (스폰 위치에 도달할 때까지 재타겟팅 차단) */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI")
    bool bIsLeashingHome = false;

    // AI 어그로 성향 (DT_EnemyData에서 자동 주입 - BP 기본값에서는 숨김)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "AI|Aggro")
    EAggroStyle AggroStyle = EAggroStyle::Preemptive;

    // ‘맞아서 어그로’ 유지 시간(초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Aggro")
    float AggroByHitHoldTime = 10.f;

    // 피격으로 어그로 켜기
    UFUNCTION(BlueprintCallable, Category = "Aggro")
    void MarkAggroByHit(AActor* InstigatorActor);

    // 읽기용
    UFUNCTION(BlueprintPure, Category = "Aggro")
    bool IsAggroByHit() const { return bAggroByHit; }

    UFUNCTION(BlueprintPure, Category = "Aggro")
    float GetLastAggroByHitTime() const { return LastAggroByHitTime; }

    UFUNCTION(BlueprintPure, Category = "Aggro")
    float GetAggroByHitHoldTime() const { return AggroByHitHoldTime; }

    //Fade (실제 처리는 CorpseComponent. 등장 페이드 사용 여부/시간만 여기 유지 — BP_Enemy_Base 설정값 보존)

    // 현재 페이드 중인가?
    UFUNCTION(BlueprintCallable, Category = "Spawn|FadeIn")
    bool IsSpawnFading() const;

    void TickCombatRotation(float DeltaSeconds);

    UPROPERTY(EditAnywhere, Category = "Spawn|FadeIn")
    bool bUseSpawnFadeIn = true;

    UPROPERTY(EditAnywhere, Category = "Spawn|FadeIn", meta = (ClampMin = "0.05", ClampMax = "20.0"))
    float SpawnFadeDuration = 0.6f;

    // 수동으로도 호출 가능 (스포너에서 호출해도 됨)
    UFUNCTION(BlueprintCallable, Category = "Spawn|FadeIn")
    void PlaySpawnFadeIn(float Duration = -1.f);

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SetInteractionOutline(bool bEnable);

    // INonInteractableInterface 구현
    virtual void Interact_Implementation(ANonCharacterBase* Interactor) override;
    virtual FText GetInteractLabel_Implementation() override;
    virtual void SetInteractHighlight_Implementation(bool bEnable) override;

protected:
    // GAS
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UNonAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY()
    TObjectPtr<UNonAttributeSet> AttributeSet;

    UPROPERTY(EditDefaultsOnly, Category = "GAS|Init")
    TSubclassOf<UGameplayEffect> GE_DefaultAttributes;

    UPROPERTY(EditDefaultsOnly, Category = "GAS|Damage")
    TSubclassOf<UGameplayEffect> GE_Damage;

    UPROPERTY(EditDefaultsOnly, Category = "GAS|Tags")
    FName DeadTagName = FName("State.Dead");

    // [New] 적 캐릭터 기본 어빌리티 (GA_HitReaction 등 등록용)
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Abilities")
    TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    TObjectPtr<UWidgetComponent> HPBarWidget;

    /** 선공 몬스터가 플레이어 발견 시 머리 위에 띄우는 경계/느낌표 위젯 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI|Alert")
    TObjectPtr<UWidgetComponent> AlertWidget;

    /** 경계 위젯을 자동으로 숨기는 타이머 함수 */
    UFUNCTION()
    void HideAlertMark();

    public:
    // [New] GAS GA_Death에서 호출할 함수들 (Public)
    UFUNCTION(BlueprintCallable, Category = "Combat|Death")
    void StartDeathSequence();

    UFUNCTION(BlueprintCallable, Category = "Combat|Death")
    void StartRagdoll();

protected:
    void HandleDeath(); // Legacy Internal
    void BindAttributeDelegates();

    // HP바 표시 로직
    FTimerHandle HPBarHideTimer;

    // [New] 로컬 HP바 표시 플래그 (Client Only)
    bool bClientHPBarVisible = false;
    FTimerHandle ClientHPBarTimer;

public:
    UFUNCTION(BlueprintCallable, Category = "UI|HPBar")
    void ShowLocalHPBar(float Duration = 3.0f);
    void HideLocalHPBar();
protected:

    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    float CombatTimeout = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "UI|HPBar")
    bool bShowHPBarOnlyInCombat = true;

    // [Reverted] Replication removed for Local Visibility Logic
    bool bInCombat = false;

    FTimerHandle CombatTimeoutTimer;

    // [New] 태그 기반 자동 히트박스 시스템
    // 에디터에서 컴포넌트 태그에 "AttackHitbox"라고 적힌 모든 콜리전을 자동으로 관리합니다.
    UPROPERTY()
    TArray<class UShapeComponent*> ActiveHitboxes;

    UPROPERTY()
    TSet<TWeakObjectPtr<AActor>> HitOnce;

    // [Legacy Removed] Multicast_PlayHitReact
    // UFUNCTION(NetMulticast, Unreliable)
    // void Multicast_PlayHitReact(EHitQuadrant Quad);
    
    // [Legacy Removed] Multicast_PlayAttack, PlayMontageSafe
    // UFUNCTION(NetMulticast, Reliable)
    // void Multicast_PlayAttack(UAnimMontage* MontageToPlay);

protected:
    // 실제 몽타주 재생 함수 (로컬) - Removed
    // void PlayMontageSafe(UAnimMontage* Montage, float Rate = 1.0f);

    UFUNCTION()
    void OnAttackHitBegin(UPrimitiveComponent* Overlapped, AActor* Other,
        UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);

    void EnterCombat();
    void LeaveCombat();
    void UpdateHPBarVisibility();


    // 상호작용 범위 콜리전
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
    USphereComponent* InteractCollision;

    // 플레이어가 죽은 적에 접근하면 이 반경 안에서 오버랩
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
    float CorpseInteractRadius = 200.f;

    // 시체 수명/루팅 상태는 CorpseComponent로 이동

private:
    void ApplyHealthDelta_Direct(float Delta);
    bool HasDeadTag() const;
    void AddDeadTag();


    bool bAggroByHit = false;
    float LastAggroByHitTime = -1000.f;

    // [Reverted] Replication removed
    bool bAggro = false;

    UPROPERTY(ReplicatedUsing = OnRep_AggroTarget)
    TObjectPtr<AActor> AggroTarget;

    // 사망 시각 (서버 월드 시간, 살아 있으면 -1). 늦게 접속한 사람에게 시체를 쓰러진 포즈로 보여 주기 위해 복제
    UPROPERTY(ReplicatedUsing = OnRep_DeathServerTime)
    float DeathServerTime = -1.f;

    UFUNCTION()
    void OnRep_DeathServerTime();

    UFUNCTION()
    void OnRep_AggroTarget(AActor* OldAggroTarget);

    void HandleAggroTargetChanged(AActor* OldTarget, AActor* NewTarget);

    // 경험치 지급용: 마지막으로 나를 공격한 플레이어
    TWeakObjectPtr<ANonCharacterBase> LastDamageInstigator;

    // 데스 몽타주 끝난 뒤 포즈 고정용
    FTimerHandle DeathPoseFreezeTimerHandle;

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnDeathCollision();

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_FreezeDeathPose();

public:
    UFUNCTION()
    virtual void FreezeDeathPose();




    // ───── 복구된 기존 기능들 ─────
    
    // 몬스터 전용 기본 공격 몽타주 목록 (DT_EnemyData에서 자동 주입 - BP 기본값에서는 숨김)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|Animation")
    TArray<TObjectPtr<class UAnimMontage>> AttackMontages;

    // 몬스터 전용 원거리 공격 몽타주 (DT_EnemyData에서 자동 주입 - BP 기본값에서는 숨김)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|Animation")
    TObjectPtr<class UAnimMontage> RangeAttackMontage = nullptr;

    // 플레이어가 너무 가까이 접근했을 때 뒤로 물러나 거리를 벌리는 카이팅/도망 여부 (DT_EnemyData에서 자동 주입)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|AI")
    bool bFleeWhenTooClose = false;

    // 뒤로 물러날 때 재생할 백스텝 회피 몽타주 (선택 사항 - 비어있으면 뒤로 자연스럽게 이동하여 거리를 벌립니다)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|Animation")
    TObjectPtr<class UAnimMontage> BackstepMontage = nullptr;

    /** 플레이어가 위험 거리(예: 350cm 이내)로 다가왔을 때 백스텝이나 후퇴(카이팅)를 시도합니다. */
    UFUNCTION(BlueprintCallable, Category = "Combat|AI")
    bool TryPerformBackstepOrFlee(AActor* TargetActor);

    /** 마지막으로 후퇴/백스텝을 발동한 시각 */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|AI")
    float LastFleeTime = -100.f;

    /** 후퇴/백스텝 재발동 쿨다운 (기본 2.0초) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|AI")
    float FleeCooldown = 2.0f;

    /** 후퇴를 발동할 위험 근접 거리 (기본 450.0cm, 플레이어가 다가올 때 민첩하게 반응) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|AI")
    float FleeTriggerDistance = 450.0f;

    /** 현재 위험 거리에서 후퇴(카이팅) 이동 중인지 여부 */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|AI")
    bool bIsFleeing = false;

    /** 후퇴 이동 명령(MoveToLocation)을 컨트롤러에 전달하는 순간인지 식별하는 내부 플래그 */
    bool bIsIssuingFleeMove = false;

    /** [하이브리드 전용] 현재 연속으로 수행한 근접 공격 횟수 */
    int32 HybridMeleeAttackCount = 0;

    /** [하이브리드 전용] 후퇴(거리 벌리기) 전까지 수행할 목표 근접 공격 횟수 (랜덤 1~3회) */
    int32 HybridTargetMeleeCount = 0;

    /** [하이브리드 전용] 목표 근접 공격 횟수를 모두 채워 즉시 후퇴해야 하는 상태 플래그 */
    bool bPendingHybridFlee = false;

    /** 후퇴 집중 만료 시각 */
    float FleeEndTime = 0.f;

    /** 현재 후퇴 진행 중인지 검사 (만료 시 bIsFleeing 자동 해제) */
    UFUNCTION(BlueprintCallable, Category = "Combat|AI")
    bool IsFleeing();

    /** 후퇴 상태 강제 종료 */
    UFUNCTION(BlueprintCallable, Category = "Combat|AI")
    void FinishFlee();

    // 몬스터 전용 피격 몽타주 저장소 (DT_EnemyData에서 자동 주입 - BP 기본값 창에서는 깔끔하게 숨김)
    UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "Combat|HitReaction")
    TMap<FGameplayTag, class UAnimMontage*> HitMontages;

    // 몬스터의 태그에 맞는 피격 몽타주 반환
    UFUNCTION(BlueprintCallable, Category = "Combat|HitReaction")
    class UAnimMontage* GetHitMontage(FGameplayTag HitTag) const;

    // 시체 상호작용 관련 (이전에 지워졌던 것들)
    void EnableCorpseInteraction();

private:
    void BindASCDelegates();
};
