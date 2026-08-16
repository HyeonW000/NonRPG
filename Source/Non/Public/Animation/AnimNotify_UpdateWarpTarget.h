#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_UpdateWarpTarget.generated.h"

/**
 * 점프 직전 모션 워핑 타겟 위치를 1회 최신 플레이어 위치로 자동 갱신해 주는 애니메이션 노티파이
 */
UCLASS(meta = (DisplayName = "Update Motion Warping Target Location"))
class NON_API UAnimNotify_UpdateWarpTarget : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
