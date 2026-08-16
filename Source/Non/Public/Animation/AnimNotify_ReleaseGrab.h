#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ReleaseGrab.generated.h"

/**
 * 잡기 성공 연출 몽타주 중간에 보스가 유저를 내팽개쳐서 손을 떼는 프레임에 소켓을 분리(Detach)하고 이동을 복구해 주는 애니메이션 노티파이
 */
UCLASS(meta = (DisplayName = "Release Boss Grab Target"))
class NON_API UAnimNotify_ReleaseGrab : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
