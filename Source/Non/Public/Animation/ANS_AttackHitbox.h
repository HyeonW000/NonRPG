#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "ANS_AttackHitbox.generated.h"

UCLASS(meta = (DisplayName = "Attack Hitbox Window"))
class NON_API UANS_AttackHitboxWindow : public UAnimNotifyState
{
    GENERATED_BODY()
public:
    // 공격 적중 시 플레이어에게 전달할 피격 리액션 태그 (기본: Effect.Hit.Light, 강타면 Effect.Hit.Heavy, 다운이면 Effect.Hit.Knockdown)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction")
    FGameplayTag HitReactionTag;

    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
};
