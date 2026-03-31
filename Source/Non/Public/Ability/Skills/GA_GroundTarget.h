// GA_GroundTarget.h
// 3단계 시전 시스템 (캐스팅 → 조준 → 시전)
// 모든 몽타주/AOE/캐스팅 파라미터는 SkillDataAsset의 FAOEConfig에서 읽음
// → GA 블루프린트 1개로 모든 장판 스킬을 처리!
#pragma once

#include "CoreMinimal.h"
#include "Ability/Skills/GA_SkillBase.h"
#include "GA_GroundTarget.generated.h"

class ADamageAOE;

/** 현재 시전 단계 */
UENUM(BlueprintType)
enum class EGroundTargetPhase : uint8
{
    Casting,    // 1단계: 캐스팅 中 (이동하면 캔슬, 피격 캔슬)
    Targeting,  // 2단계: 조준 中 (데칼, 이동/피격 캔슬)
    Releasing,  // 3단계: 시전 中 (격발 애니 → AOE 스폰)
};

UCLASS(Abstract)
class NON_API UGA_GroundTarget : public UGA_SkillBase
{
    GENERATED_BODY()

public:
    UGA_GroundTarget();

protected:
    // ── 이벤트 훅 (블루프린트 연동용) ────────────────────────────

    /** 1단계 시작 → 캐스팅 바 UI 시작용 */
    UFUNCTION(BlueprintNativeEvent, Category = "GroundTarget|Events")
    void OnCastingStarted(float Duration);
    virtual void OnCastingStarted_Implementation(float Duration) {}

    /** 1단계 완료 */
    UFUNCTION(BlueprintNativeEvent, Category = "GroundTarget|Events")
    void OnCastingCompleted();
    virtual void OnCastingCompleted_Implementation() {}

    /** 어느 단계에서든 캔슬됐을 때 */
    UFUNCTION(BlueprintNativeEvent, Category = "GroundTarget|Events")
    void OnSkillCancelled();
    virtual void OnSkillCancelled_Implementation() {}

    /** AOE 스폰 직후 자식 클래스 추가 처리 훅 */
    UFUNCTION(BlueprintNativeEvent, Category = "GroundTarget|Events")
    void OnAOESpawned(ADamageAOE* SpawnedAOE, const FVector& TargetLocation);
    virtual void OnAOESpawned_Implementation(ADamageAOE* SpawnedAOE, const FVector& TargetLocation) {}

    // ── 오버라이드 ───────────────────────────────────────────────

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

private:
    // ── 런타임 상태 ──────────────────────────────────────────────

    EGroundTargetPhase CurrentPhase = EGroundTargetPhase::Casting;

    UPROPERTY()
    TWeakObjectPtr<AActor> ActiveDecal;

    FVector ConfirmedTargetLocation = FVector::ZeroVector;

    FTimerHandle CastTimerHandle;
    FTimerHandle CastingTickHandle;
    FTimerHandle TargetingTickHandle;

    FDelegateHandle HitTagEventHandle;

    const struct FSkillRow* CachedRow = nullptr;

    // ── 내부 함수 ────────────────────────────────────────────────

    /** [1단계] 캐스팅 시작 */
    void StartCastingPhase();

    /** [1단계] 캐스팅 완료 타이머 콜백 */
    void OnCastingTimerExpired();

    /** [1단계] 이동 감지 Tick (이동 시 스킬 캔슬) */
    void CastingTick();

    /** [1단계] 피격 캔슬 감지 등록/해제 */
    void RegisterHitCancelListener();
    void UnregisterHitCancelListener();
    void OnHitTagChanged(const FGameplayTag Tag, int32 NewCount);

    /** 입력 액션: 좌클릭(확정) */
    UFUNCTION()
    void OnConfirmTargetInput(float TimeWaited);

    /** 입력 액션: 우클릭(취소) */
    UFUNCTION()
    void OnCancelTargetInput(float TimeWaited);


    /** [2단계] 조준 시작 */
    void StartTargetingPhase();

    /** [2단계] 20fps Tick: 데칼 위치 갱신 + 클릭/이동 감지 */
    void TargetingTick();

    /** [2단계] 좌클릭 확정 */
    void OnConfirmTarget();

    /** [모든 단계] 취소 처리 */
    void OnCancelTarget();

    /** [3단계] 격발 몽타주 재생 → AOE 스폰 */
    void StartReleasingPhase();

    UFUNCTION() void OnReleaseMontageFinished();
    UFUNCTION() void OnReleaseMontageCancelled();

    /** 최종 AOE 스폰 (BeginPlay 전 설정 보장) */
    void SpawnAOE();

    // ── 유틸리티 ─────────────────────────────────────────────────
    bool GetMouseGroundLocation(FVector& OutLocation, float MaxRange) const;
    void SpawnDecal(const FVector& Location, TSubclassOf<AActor> DecalClass);
    void DestroyDecal();
};
