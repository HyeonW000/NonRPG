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

    // 그래프에서 쓸 로코모션 블렌드스페이스 (AnimSet에서 채움)
    UPROPERTY(BlueprintReadOnly, Category = "AnimAssets")
    TObjectPtr<class UBlendSpace>     LocomotionBS = nullptr;

protected:
    TWeakObjectPtr<class ACharacter> OwnerChar;
    TWeakObjectPtr<class UCharacterMovementComponent> MoveComp;

    void PullAnimSetFromOwner();
};
