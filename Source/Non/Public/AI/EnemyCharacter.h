#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Combat/CombatTypes.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnemyCharacter.generated.h"

class UAbilitySystemComponent;
class UNonAbilitySystemComponent;
class UNonAttributeSet;
class UGameplayEffect;
class UWidgetComponent;
class UEnemyAnimSet;
class ADamageNumberActor;
class UBoxComponent;

UENUM(BlueprintType)
enum class EAggroStyle : uint8
{
    Preemptive UMETA(DisplayName = "Preemptive"), // 근접/시야 등으로 먼저 어그로
    Reactive   UMETA(DisplayName = "Reactive")   // 맞아야만 어그로
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDied, AEnemyCharacter*, Enemy);

UCLASS()
class NON_API AEnemyCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AEnemyCharacter();

    // IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnEnemyDied OnEnemyDied;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    bool bDied = false;

public:
    // 능력치 초기화(GE 적용)
    void InitializeAttributes();

    // HP바 갱신
    void UpdateHPBar() const;

    // 데미지 적용 (양수=피해)
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ApplyDamage(float Amount, AActor* DamageInstigator);

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsDead() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ApplyDamageAt(float Amount, AActor* DamageInstigator, const FVector& WorldLocation);

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SpawnDamageNumber(float Amount, FVector WorldLocation, bool bIsCritical);

    UPROPERTY(EditDefaultsOnly, Category = "UI|Damage")
    TSubclassOf<ADamageNumberActor> DamageNumberActorClass;

    // ───── Aggro/공격 ─────
    UPROPERTY(EditDefaultsOnly, Category = "AI")
    float AttackRange = 220.f;

    void SetAggro(bool bNewAggro);
    void TryStartAttack();

    UFUNCTION(BlueprintCallable, Category = "Combat|Attack")
    void PlayAttackMontage();

    UFUNCTION(BlueprintPure, Category = "Combat|Attack")
    UAnimMontage* PickAttackMontage() const;

    // 히트박스 On/Off (AnimNotify에서 호출)
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void AttackHitbox_Enable();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void AttackHitbox_Disable();

    // ───── Hit React ─────
    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact")
    float HitReactCooldown = 0.3f;

    FTimerHandle HitReactCDTimer;
    bool bCanHitReact = true;

    UFUNCTION(BlueprintCallable, Category = "Combat|HitReact")
    void OnGotHit(float Damage, AActor* InstigatorActor, const FVector& ImpactPoint);

    EHitQuadrant ComputeHitQuadrant(const FVector& ImpactPoint) const;
    void PlayHitReact(EHitQuadrant Quad);

    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact")
    TObjectPtr<UAnimMontage> HitReact_F = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact")
    TObjectPtr<UAnimMontage> HitReact_B = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact")
    TObjectPtr<UAnimMontage> HitReact_L = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact")
    TObjectPtr<UAnimMontage> HitReact_R = nullptr;

    // ── Knockback(피격 넉백) ──
    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact")
    EKnockbackMode KnockbackMode = EKnockbackMode::Launch;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact", meta = (EditCondition = "KnockbackMode==EKnockbackMode::Launch"))
    float LaunchStrength = 500.f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact", meta = (EditCondition = "KnockbackMode==EKnockbackMode::Launch"))
    float LaunchUpward = 0.f;

    // 넉백/히트 방향 계산 헬퍼
    FVector ComputeKnockbackDir(AActor* InstigatorActor, const FVector& ImpactPoint) const;

    void ApplyHitStopSelf(float Scale, float Duration);
    FTimerHandle HitStopTimer;

    // 공격 딜레이(피격 후)
    UPROPERTY(EditDefaultsOnly, Category = "Combat|AI")
    float AttackDelayAfterHit = 0.6f;   // 피격 후 N초 동안 공격 금지

    // 다음 공격 가능 시각(TimeSeconds)
    float NextAttackAllowedTime = 0.f;

    // 사정거리 진입 후 첫 공격까지 대기
    UPROPERTY(EditDefaultsOnly, Category = "Combat|AI")
    float FirstAttackWindup = 0.6f;  // 사정거리 들어온 뒤 첫 휘두르기까지 쉬는 시간

    // 내부 상태
    float EnterRangeTime = -1.f;

    // 진입 시각 마킹/검사
    void MarkEnteredAttackRange();
    bool IsFirstAttackWindupDone() const;


    // 경직(이동 멈춤) 옵션
    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact")
    float HitMovePauseDuration = 0.25f;         // 피격 후 이동 멈추는 시간

    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HitMoveSpeedScale = 0.0f;             // 0=완전 정지, 0.3=느려짐

    FTimerHandle HitMovePauseTimer;

    void StartHitMovePause();
    void EndHitMovePause();

    UFUNCTION(BlueprintCallable, Category = "Combat|AI")
    void BlockAttackFor(float Seconds);

    UFUNCTION(BlueprintPure, Category = "Combat|AI")
    bool IsAttackAllowed() const;

    // Death
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Death")
    bool bUseDeathMontage = false;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Death", meta = (EditCondition = "bUseDeathMontage"))
    TObjectPtr<UAnimMontage> DeathMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|HitReact")
    FName HitReactSlotName = TEXT("DefaultSlot");

    UFUNCTION(BlueprintPure, Category = "Animation")
    UEnemyAnimSet* GetAnimSet() const { return AnimSet; }

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

    // 수동으로도 호출 가능 (스포너에서 호출해도 됨)
    UFUNCTION(BlueprintCallable, Category = "Spawn|FadeIn")
    void PlaySpawnFadeIn(float Duration = -1.f);


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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    TObjectPtr<UWidgetComponent> HPBarWidget;

    void HandleDeath();
    void BindAttributeDelegates();

    // HP바 표시 로직
    UPROPERTY(EditDefaultsOnly, Category = "UI|HPBar")
    float HPBarVisibleTimeAfterHit = 2.0f;

    FTimerHandle HPBarHideTimer;

    UPROPERTY(EditDefaultsOnly, Category = "Combat")
    float CombatTimeout = 3.0f;

    UPROPERTY(EditDefaultsOnly, Category = "UI|HPBar")
    bool bShowHPBarOnlyInCombat = true;

    bool bInCombat = false;
    FTimerHandle CombatTimeoutTimer;

    // 히트박스
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    UBoxComponent* AttackHitbox;

    UPROPERTY()
    TSet<TWeakObjectPtr<AActor>> HitOnce;

    UFUNCTION()
    void OnAttackHitBegin(UPrimitiveComponent* Overlapped, AActor* Other,
        UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);

    void EnterCombat();
    void LeaveCombat();
    void UpdateHPBarVisibility();

    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    TObjectPtr<class UEnemyAnimSet> AnimSet = nullptr;

private:
    void ApplyHealthDelta_Direct(float Delta);
    bool HasDeadTag() const;
    void AddDeadTag();


    bool  bAggroByHit = false;
    float LastAggroByHitTime = -1000.f;

    bool bAggro = false;

    //Fade
    void InitSpawnFadeMIDs();

    UPROPERTY(Transient)
    TArray<TObjectPtr<class UMaterialInstanceDynamic>> SpawnFadeMIDs;

    FTimerHandle SpawnFadeTimer;
    bool  bSpawnFadeActive = false;
    float SpawnFadeStartTime = 0.f;
    float SpawnFadeLen = 0.f;

    // 이동 재개용 저장
    float SavedMaxWalkSpeed = 0.f;
};
