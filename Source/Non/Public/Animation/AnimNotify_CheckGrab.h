#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CheckGrab.generated.h"

/**
 * 보스 잡기 판정을 몽타주 특정 프레임에서 100% C++로 자동 실행해 주는 애니메이션 노티파이
 */
UCLASS(meta = (DisplayName = "Check Boss Grab Target"))
class NON_API UAnimNotify_CheckGrab : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
