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
class UEnemyAnimSet;
class ADamageNumberActor;
class UBoxComponent;
class USphereComponent;
class UEnemyDataAsset;
class UEnemyDataAsset;
class ANonCharacterBase;
class UGameplayAbility; // [Fix] Forward declaration

UENUM(BlueprintType)
enum class EAggroStyle : uint8
{
    Preemptive UMETA(DisplayName = "Preemptive"), // 근접/시야 등으로 먼저 어그로
    Reactive   UMETA(DisplayName = "Reactive")   // 맞아야만 어그로
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDied, AEnemyCharacter*, Enemy);

UCLASS()
class NON_API AEnemyCharacter : public ACharacter, public IAbilitySystemInterface, public INonInteractableInterface
{
    GENERATED_BODY()

public:
    AEnemyCharacter();

    // IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnEnemyDied OnEnemyDied;

    // [New] (타겟 프레임용) AttributeSet 접근자
    UFUNCTION(BlueprintCallable, Category = "GAS")
    class UNonAttributeSet* GetAttributeSet() const { return AttributeSet; }

    // [New] (타겟 프레임용) 이름 반환
    UFUNCTION(BlueprintCallable, Category = "Config")
    FString GetEnemyName() const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    bool bDied = false;

public:
    // 능력치 초기화(GE 적용)
    void InitializeAttributes();

    // HP바 갱신
    void UpdateHPBar() const;

    //  EnemyDataAsset으로부터 값 세팅
    UFUNCTION(BlueprintCallable, Category = "Config")
    void InitFromDataAsset(const UEnemyDataAsset* InData);

    // [Restored] 데미지 숫자 표시 (Base와 별도로 존재)
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SpawnDamageNumber(float Amount, FVector WorldLocation, bool bIsCritical);

    UPROPERTY(EditDefaultsOnly, Category = "UI|Damage")
    TSubclassOf<ADamageNumberActor> DamageNumberActorClass;

    // 데미지 적용 (양수=피해)
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ApplyDamage(float Amount, AActor* DamageInstigator);

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsDead() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ApplyDamageAt(float Amount, AActor* DamageInstigator, const FVector& WorldLocation, bool bIsCritical = false, FGameplayTag ReactionTag = FGameplayTag());



    /* 상위 클래스로 이동됨
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SpawnDamageNumber(float Amount, FVector WorldLocation, bool bIsCritical);

    UPROPERTY(EditDefaultsOnly, Category = "UI|Damage")
    TSubclassOf<ADamageNumberActor> DamageNumberActorClass;
    */
    
    // ───── Aggro/공격 ─────

    // ───── Aggro/공격 ─────
    UPROPERTY(EditDefaultsOnly, Category = "AI")
    float AttackRange = 220.f;

    // 적 기본 공격 타입/계수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack")
    ENonDamageType AttackDamageType = ENonDamageType::Physical;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack", meta = (ClampMin = "0.0"))
    float AttackPowerScale = 1.0f;

    void SetAggro(bool bNewAggro);
    void TryStartAttack();

    // [Legacy Removed] PlayAttackMontage, PickAttackMontage (Moved to GA_EnemyAttack)

    // 히트박스 On/Off (AnimNotify에서 호출)
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void AttackHitbox_Enable();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void AttackHitbox_Disable();

    // ───── Hit React (Legacy Removed) ─────
    // GAS로 이관됨 (GA_HitReaction)

    // Attacks checking logic using AnimNotify is still required below
    
    // 공격 딜레이(피격 후)
    UPROPERTY(EditDefaultsOnly, Category = "Combat|AI")
    float AttackDelayAfterHit = 0.6f;   // 피격 후 N초 동안 공격 금지

    // 다음 공격 가능 시각(TimeSeconds)
    float NextAttackAllowedTime = 0.f;

    // 사정거리 진입 후 첫 공격까지 대기 (최소~최대 랜덤)
    UPROPERTY(EditDefaultsOnly, Category = "Combat|AI")
    float FirstAttackWindupMin = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|AI")
    float FirstAttackWindupMax = 1.2f;

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

    // Death (Legacy Removed - Handled by GA_Death)
    // UPROPERTY(VisibleInstanceOnly, Transient, Category = "Combat|Death")
    // bool bUseDeathMontage = false;
    // UPROPERTY(VisibleInstanceOnly, Transient, Category = "Combat|Death")
    // TObjectPtr<UAnimMontage> DeathMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact")
    FName HitReactSlotName = TEXT("DefaultSlot");

    UFUNCTION(BlueprintPure, Category = "Animation")
    class UEnemyAnimSet* GetAnimSet() const { return AnimSet; }


    // ---- EnemyDataAsset에서 온 설정값들 ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_EnemyData, Category = "Config")
    const UEnemyDataAsset* EnemyData = nullptr;

    UFUNCTION()
    void OnRep_EnemyData();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
    float BaseAttack = 10.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
    float BaseDefense = 5.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward")
    float ExpReward = 10.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI")
    FVector SpawnLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Aggro")
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

    //Fade

    // 페이드 중엔 AI/이동을 멈출지
    UPROPERTY(EditAnywhere, Category = "Spawn|FadeIn")
    bool bPauseAIWhileFading = true;

    // (선택) 페이드 끝나면 자동으로 재개
    UPROPERTY(EditAnywhere, Category = "Spawn|FadeIn")
    bool bAutoResumeAfterFade = true;

    // 현재 페이드 중인가?
    UFUNCTION(BlueprintCallable, Category = "Spawn|FadeIn")
    bool IsSpawnFading() const { return bSpawnFadeActive; }

    // 페이드 시작/틱/종료
    void BeginSpawnFade(float DurationSeconds);
    void TickSpawnFade();
    void OnSpawnFadeFinished();


    UPROPERTY(EditAnywhere, Category = "Spawn|FadeIn")
    bool bUseSpawnFadeIn = true;

    UPROPERTY(EditAnywhere, Category = "Spawn|FadeIn", meta = (ClampMin = "0.05", ClampMax = "20.0"))
    float SpawnFadeDuration = 0.6f;

    UPROPERTY(EditAnywhere, Category = "Spawn|FadeIn")
    FName FadeParamName = TEXT("Fade");

    UPROPERTY(EditAnywhere, Category = "Spawn|FadeOut", meta = (ClampMin = "0.5", ClampMax = "10.0"))
    float CorpseFadeOutDuration = 3.0f;

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

    // 히트박스
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    UBoxComponent* AttackHitbox;

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

    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    TObjectPtr<class UEnemyAnimSet> AnimSet = nullptr;

    // 상호작용 범위 콜리전
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
    USphereComponent* InteractCollision;

    // 플레이어가 죽은 적에 접근하면 이 반경 안에서 오버랩
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
    float CorpseInteractRadius = 200.f;

    // 루팅 안 해도 시체가 유지되는 시간
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
    float CorpseLifeTime = 120.f;

    // 루팅 후 시체 유지 시간 (0이면 바로 삭제)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
    float CorpseLifeTimeAfterLoot = 3.f;

    // 현재 루팅 가능한 상태인가
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
    bool bLootAvailable = false;

    // 이미 루팅했는가
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
    bool bLooted = false;

    FTimerHandle CorpseRemoveTimerHandle;

    UFUNCTION()
    void OnInteractOverlapBegin(
        UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnInteractOverlapEnd(
        UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    void StartCorpseTimer(float LifeTime);
    void OnCorpseExpired();

private:
    void ApplyHealthDelta_Direct(float Delta);
    bool HasDeadTag() const;
    void AddDeadTag();


    bool bAggroByHit = false;
    float LastAggroByHitTime = -1000.f;

    // [Reverted] Replication removed
    bool bAggro = false;

    // 경험치 지급용: 마지막으로 나를 공격한 플레이어
    TWeakObjectPtr<ANonCharacterBase> LastDamageInstigator;

    //Fade
    void InitSpawnFadeMIDs();

    UPROPERTY(Transient)
    TArray<TObjectPtr<class UMaterialInstanceDynamic>> SpawnFadeMIDs;

    FTimerHandle SpawnFadeTimer;
    bool  bSpawnFadeActive = false;
    float SpawnFadeStartTime = 0.f;
    float SpawnFadeLen = 0.f;
    bool bIsFadingOut = false;

    // 수동으로 페이드 아웃 시작 (멀티캐스트로 변경)
    UFUNCTION(NetMulticast, Reliable)
    void PlaySpawnFadeOut(float Duration = 1.0f);

    // 이동 재개용 저장
    float SavedMaxWalkSpeed = 0.f;

    // 데스 몽타주 끝난 뒤 포즈 고정용
    FTimerHandle DeathPoseFreezeTimerHandle;

    UFUNCTION()
    void FreezeDeathPose();

    void EnableCorpseInteraction();
};
