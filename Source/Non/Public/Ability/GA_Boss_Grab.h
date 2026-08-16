#pragma once

#include "CoreMinimal.h"
#include "Ability/GA_EnemyAttack.h"
#include "GA_Boss_Grab.generated.h"

/**
 * 보스 잡기(Grab) 어빌리티
 * 1. 시도 몽타주(AttemptMontage) 재생
 * 2. 손 뻗는 순간(AnimNotify 또는 타이머) 충돌체 검사로 플레이어 감지
 * 3. 성공 시: 성공 몽타주(SuccessMontage) 재생 + 플레이어를 보스 손 소켓에 부착(Attach)
 * 4. 실패 시: 헛손질 모션 재생 후 스킬 종료
 */
UCLASS()
class NON_API UGA_Boss_Grab : public UGA_EnemyAttack
{
    GENERATED_BODY()

public:
    UGA_Boss_Grab();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    // 애니메이션 노티파이 또는 블루프린트에서 호출하여 잡기 판정을 수행하는 함수
    UFUNCTION(BlueprintCallable, Category = "Boss|Grab")
    bool CheckAndExecuteGrab();

    // 몽타주 내팽개치는 시점에 호출하여 유저를 손 소켓에서 분리(Detach)하고 던지는 함수
    UFUNCTION(BlueprintCallable, Category = "Boss|Grab")
    void ReleaseGrabbedVictim();

protected:
    virtual void OnMontageEnded() override;
    virtual void OnMontageCancelled() override;

    // 잡기 시도 + 실패 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grab")
    TObjectPtr<UAnimMontage> AttemptMontage;

    // 잡기 성공 전용 연출 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grab")
    TObjectPtr<UAnimMontage> SuccessMontage;

    // 에디터 디테일 창에서 선택하는 잡기 이벤트 태그 (기본값: Event.Grabbed)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grab")
    FGameplayTag GrabEventTag;

    // 에디터 디테일 창에서 선택하는 잡힘 상태 태그 (기본값: State.Grabbed)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grab")
    FGameplayTag GrabbedStateTag;

    // 플레이어를 잡아서 부착할 보스 손 소켓 이름 (기본값: hand_r_Socket)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grab")
    FName HandSocketName = FName("hand_r_Socket");

    // 잡기 판정 구체(Sphere) 반지름 (기본값: 150cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grab")
    float GrabCheckRadius = 150.0f;

    // 잡기 판정 위치 전방 거리 오프셋 (기본값: 180cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grab")
    float GrabCheckForwardOffset = 180.0f;

    // 잡기 판정 위치 높이 오프셋 (기본값: 50cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grab")
    float GrabCheckHeightOffset = 50.0f;

    // 잡기 성공 시 플레이어에게 적용할 데미지 GameplayEffect
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Grab")
    TSubclassOf<UGameplayEffect> GrabDamageEffectClass;

private:
    UPROPERTY()
    TObjectPtr<AActor> GrabbedVictim;

    bool bIsGrabSuccessful = false;

    UFUNCTION()
    void OnSuccessMontageEnded();
};
