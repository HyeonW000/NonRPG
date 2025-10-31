#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "ANS_DodgeIFrame.generated.h"

UCLASS(meta = (DisplayName = "Dodge I-Frame"))
class NON_API UANS_DodgeIFrame : public UAnimNotifyState
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "IFrame")
    bool bDisablePawnCollision = false;

    UPROPERTY(EditAnywhere, Category = "IFrame")
    bool bAddIFrameTag = true;

    virtual void NotifyBegin(
        USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        float TotalDuration,
        const FAnimNotifyEventReference& EventReference
    ) override;

    virtual void NotifyEnd(
        USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference
    ) override;
};
