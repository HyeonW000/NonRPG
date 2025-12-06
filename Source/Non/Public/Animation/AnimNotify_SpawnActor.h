#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SpawnActor.generated.h"

class UCameraShakeBase;

/**
 * 애니메이션 도중 특정 액터(블루프린트)를 소환하는 범용 노티파이
 */
UCLASS(meta = (DisplayName = "Spawn Actor (Generic)"))
class NON_API UAnimNotify_SpawnActor : public UAnimNotify
{
    GENERATED_BODY()
public:
    // 소환할 액터 클래스 (예: BP_BoxAOE)
    UPROPERTY(EditAnywhere, Category = "SpawnSettings")
    TSubclassOf<AActor> ActorClass;

    // 소환 위치 기준 소켓 (없으면 캐릭터 발 밑/루트)
    UPROPERTY(EditAnywhere, Category = "SpawnSettings")
    FName SocketName = NAME_None;

    // 위치 오프셋 (소켓 기준)
    UPROPERTY(EditAnywhere, Category = "SpawnSettings")
    FVector LocationOffset = FVector::ZeroVector;

    // 회전 오프셋 (소켓 기준)
    UPROPERTY(EditAnywhere, Category = "SpawnSettings")
    FRotator RotationOffset = FRotator::ZeroRotator;

    // BoxAOE 전용: 박스 크기 덮어쓰기
    UPROPERTY(EditAnywhere, Category = "SpawnSettings|BoxAOE")
    bool bOverrideBoxExtent = false;

    UPROPERTY(EditAnywhere, Category = "SpawnSettings|BoxAOE", meta = (EditCondition = "bOverrideBoxExtent"))
    FVector BoxExtent = FVector(32.f, 32.f, 32.f);

    // 소환된 액터를 소켓에 계속 붙여둘지 여부
    UPROPERTY(EditAnywhere, Category = "SpawnSettings")
    bool bAttachToSocket = false;

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

    /** 카메라 셰이크 클래스 (선택) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    TSubclassOf<UCameraShakeBase> CameraShakeClass;

    /** 셰이크 세기 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float CameraShakeScale = 1.f;
};
