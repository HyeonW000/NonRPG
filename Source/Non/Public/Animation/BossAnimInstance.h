#pragma once

#include "CoreMinimal.h"
#include "Animation/EnemyAnimInstance.h"
#include "BossAnimInstance.generated.h"

/**
 * ABossCharacter와 연동되는 전용 애니메이션 인스턴스 클래스
 */
UCLASS()
class NON_API UBossAnimInstance : public UEnemyAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    // 현재 보스의 페이즈 (1, 2, 3...)
    UPROPERTY(BlueprintReadOnly, Category = "BossAnim")
    int32 BossPhase = 1;

    // 페이즈 전환 중(포효 등)인지 여부
    UPROPERTY(BlueprintReadOnly, Category = "BossAnim")
    bool bIsTransitioning = false;

    // [New] 캐릭터 정면과 목표 방향의 각도 차이 (-180 ~ 180)
    UPROPERTY(BlueprintReadOnly, Category = "BossAnim")
    float RootYawOffset = 0.f;

    // [New] 회전 애니메이션이 재생 중인지 여부 (ABP에서 활용)
    UPROPERTY(BlueprintReadWrite, Category = "BossAnim")
    bool bIsTurning = false;

protected:
    TWeakObjectPtr<class ABossCharacter> BossChar;
};
