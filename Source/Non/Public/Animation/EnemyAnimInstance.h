#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/BlendSpace.h"
#include "EnemyAnimInstance.generated.h"

UCLASS(Blueprintable, BlueprintType)
class NON_API UEnemyAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    // AnimGraph/BP에서 읽어 쓸 변수
    UPROPERTY(BlueprintReadOnly, Category = "AnimVars", meta = (AllowPrivateAccess = "true"))
    float Speed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "AnimVars", meta = (AllowPrivateAccess = "true"))
    bool bInAir = false;

    UPROPERTY(BlueprintReadOnly, Category = "AnimVars", meta = (AllowPrivateAccess = "true"))
    bool bIsAccelerating = false;


protected:
    TWeakObjectPtr<class ACharacter> OwnerChar;
    TWeakObjectPtr<class UCharacterMovementComponent> MoveComp;

};
