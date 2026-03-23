#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_WeaponTrail.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

UCLASS()
class NON_API UANS_WeaponTrail : public UAnimNotifyState {
  GENERATED_BODY()

public:
  UANS_WeaponTrail();

  virtual void
  NotifyBegin(USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation,
              float TotalDuration,
              const FAnimNotifyEventReference &EventReference) override;
  virtual void
  NotifyTick(USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation,
             float FrameDeltaTime,
             const FAnimNotifyEventReference &EventReference) override;
  virtual void
  NotifyEnd(USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation,
            const FAnimNotifyEventReference &EventReference) override;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
  UNiagaraSystem *TrailSystem;

private:
  // 헬퍼 함수: 태그를 통해 무기 액터에 붙어있는 현재 나이아가라 컴포넌트 찾기
  UNiagaraComponent *FindTrailComponent(class AActor *WeaponActor) const;
};
