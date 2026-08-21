#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_IgnorePawnCollision.generated.h"

/**
 * 몽타주 노티파이 구간 동안 캐릭터 간 캡슐 물리 충돌 비벼짐(떨림)을 일시 무시시키는 AnimNotifyState
 */
UCLASS(meta = (DisplayName = "Ignore Pawn Collision"))
class NON_API UANS_IgnorePawnCollision : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UANS_IgnorePawnCollision();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	UPROPERTY()
	TWeakObjectPtr<AActor> CachedOwner;

	UPROPERTY()
	TWeakObjectPtr<AActor> CachedTarget;
};
