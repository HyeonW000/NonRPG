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

protected:
    TWeakObjectPtr<class ABossCharacter> BossChar;
};
