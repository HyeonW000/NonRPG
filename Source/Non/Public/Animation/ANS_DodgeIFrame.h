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

    // 저스트 회피 판정 시작 시각을 기록할지. 회피 몽타주는 켜 두고,
    // 넉다운 기상 무적처럼 회피가 아닌 무적 구간에 쓸 때는 꺼야 기상 중 피격이 저스트 회피로 판정되지 않음
    UPROPERTY(EditAnywhere, Category = "IFrame")
    bool bRecordJustDodge = true;

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
