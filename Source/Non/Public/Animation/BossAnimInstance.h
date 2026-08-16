#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BossAnimInstance.generated.h"

UCLASS()
class NON_API UBossAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadOnly, Category = "BossAnim")
    int32 BossPhase = 1;

    UPROPERTY(BlueprintReadOnly, Category = "BossAnim")
    bool bIsTransitioning = false;

    UPROPERTY(BlueprintReadOnly, Category = "BossAnim")
    float Speed = 0.f;

protected:
    TWeakObjectPtr<class ABossCharacter> BossChar;
};
