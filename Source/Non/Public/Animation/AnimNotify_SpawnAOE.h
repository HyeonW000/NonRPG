#pragma once
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Combat/DamageSphereAOE.h"
#include "AnimNotify_SpawnAOE.generated.h"

UCLASS(meta = (DisplayName = "Spawn AOE (DamageSphereAOE)"))
class NON_API UAnimNotify_SpawnAOE : public UAnimNotify
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "AOE")
    TSubclassOf<ADamageSphereAOE> AOEClass;

    UPROPERTY(EditAnywhere, Category = "AOE")
    float Radius = 180.f;

    UPROPERTY(EditAnywhere, Category = "AOE")
    float Damage = 15.f;

    UPROPERTY(EditAnywhere, Category = "AOE")
    float Duration = 0.6f;

    UPROPERTY(EditAnywhere, Category = "AOE")
    float Interval = 0.12f;

    UPROPERTY(EditAnywhere, Category = "AOE")
    bool bSingleHitPerActor = true;

    UPROPERTY(EditAnywhere, Category = "AOE")
    ETeamSide Team = ETeamSide::Enemy;

    UPROPERTY(EditAnywhere, Category = "Attach")
    bool bAttachToSocket = true;

    UPROPERTY(EditAnywhere, Category = "Attach", meta = (EditCondition = "bAttachToSocket"))
    FName SocketName = NAME_None;

    UPROPERTY(EditAnywhere, Category = "Attach", meta = (EditCondition = "bAttachToSocket"))
    FVector RelativeOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDebugDraw = false;

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
