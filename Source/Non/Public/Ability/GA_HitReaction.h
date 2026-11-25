#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_HitReaction.generated.h"

/**
 * 피격 리액션 어빌리티
 *  - Trigger: Effect.Hit.*
 */
UCLASS()
class NON_API UGA_HitReaction : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UGA_HitReaction();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, 
                                 const FGameplayAbilityActorInfo* ActorInfo, 
                                 const FGameplayAbilityActivationInfo ActivationInfo, 
                                 const FGameplayEventData* TriggerEventData) override;
	
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, 
                            const FGameplayAbilityActorInfo* ActorInfo, 
                            const FGameplayAbilityActivationInfo ActivationInfo, 
                            bool bReplicateEndAbility, bool bWasCancelled) override;

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual bool ShouldActivateAbility(ENetRole Role) const override;
protected:

    /** 피격/스턴 종료 후, 즉시 반격하지 않고 대기할 시간 (초) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact", meta = (ClampMin = "0.0"))
    float PostReactionAttackDelay = 0.5f;

    // ── [Knockback Settings] 물리 넉백 제어 ──
    /** 피격 시 공격자 반대 방향으로 밀려나는 넉백 물리 효과 사용 여부 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockback")
    bool bEnableKnockback = true;

    /** 약피격 (Light) 수평 넉백 속도 (스냅감 있게 팍 튕겨나가도록 상향) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockback")
    float LightKnockbackSpeed = 800.f;

    /** 약피격 (Light) 수직 넉백 속도 (공중에 뜨면 스윽 미끄러지므로 0 유지) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockback")
    float LightKnockbackVerticalSpeed = 0.f;

    /** 강피격 / 중피격 (Heavy/Medium) 수평 넉백 속도 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockback")
    float HeavyKnockbackSpeed = 1100.f;

    /** 강피격 / 중피격 (Heavy/Medium) 수직 넉백 속도 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockback")
    float HeavyKnockbackVerticalSpeed = 0.f;

    /** 넉다운 (Knockdown) 수평 넉백 속도 - 쓰러질 때 콤보 연계를 위해 제자리(또는 반 보 정도)에서 쓰러지도록 작게 설정 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockback")
    float KnockdownHorizontalSpeed = 150.f;

    /** 넉다운 (Knockdown) 수직 넉백 속도 - 바닥에 눕는 애니메이션이므로 0 유지 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockback")
    float KnockdownVerticalSpeed = 0.f;

    /** 넉백 / 날리기 (Knockback) 수평 날아감 속도 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockback")
    float KnockbackHorizontalSpeed = 900.f;

    /** 넉백 / 날리기 (Knockback) 수직(에어본) 넉백 속도 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockback")
    float KnockbackVerticalSpeed = 350.f;

    /** 스냅 넉백 사용 여부 (밀려난 후 지정 시간 뒤 즉시 관성을 없애 '스윽' 미끄러지지 않고 '틱!' 급정거) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockback")
    bool bUseSnapKnockback = true;

    /** 넉백 후 급제동까지 걸리는 시간 (초 단위, 짧을수록 빠르게 틱 밀리고 딱 멈춤) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockback", meta = (ClampMin = "0.01", ClampMax = "0.5"))
    float KnockbackDuration = 0.12f;

    /** 보스 몬스터 피격 시 넉백 배율 (거대 보스 슈퍼아머 연출용, 기본 0.1) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockback")
    float BossKnockbackScale = 0.1f;

    /** 넉다운 몽타주에 Loop/End 섹션이 있을 때 누워 있는 시간(초).
     *  Loop 섹션에 들어간 뒤 이 시간이 지나면 End(일어남) 섹션으로 넘어감. 0이면 섹션 연결 그대로 재생 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockdown", meta = (ClampMin = "0.0"))
    float KnockdownLieDuration = 1.0f;

    /** 넉다운 몽타주의 누워 있는 구간 섹션 이름 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockdown")
    FName KnockdownLoopSectionName = FName("Loop");

    /** 넉다운 몽타주의 일어나는 구간 섹션 이름 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HitReact|Knockdown")
    FName KnockdownEndSectionName = FName("End");

    FTimerHandle KnockdownGetUpTimerHandle;

    // 누워 있는 시간이 끝나면 End 섹션으로 넘김 (아직 Loop 섹션일 때만)
    void OnKnockdownGetUp();

    // 공격자가 보낸 커스텀 스턴(딜레이) 시간을 저장하는 변수
    float ActualReactionDelay = 0.f;

    // Helper: 몽타주 끝날 때까지 대기
    UFUNCTION()
    void OnMontageEnded();

    UFUNCTION()
    void OnMontageInterrupted();

    UFUNCTION()
    void OnMontageCancelled();

    UFUNCTION()
    void OnMontageCompleted();

    TWeakObjectPtr<UAnimMontage> CurrentMontage;
    bool bTargetIsPlayer = false;

    // [New] State.HitReacting 태그 개수 변동 시 콜백
    void OnStunTagChanged(const FGameplayTag Tag, int32 NewCount);

    FDelegateHandle TagEventHandle;
};
