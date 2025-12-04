#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_AttackHitbox.generated.h"

UCLASS(meta = (DisplayName = "Attack Hitbox Window"))
class NON_API UANS_AttackHitboxWindow : public UAnimNotifyState
{
    GENERATED_BODY()
public:
    // 필요하면 이 창에서만 임시로 박스 크기/오프셋을 바꾸는 옵션도 넣을 수 있음.
    // 우선은 On/Off만.

    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
};
