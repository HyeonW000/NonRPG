#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Combat/DamageAOE.h" // EAOEShape 사용을 위해 포함
#include "GameplayTagContainer.h" 
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

    // ── 형태 오버라이드 ──
    UPROPERTY(EditAnywhere, Category = "SpawnSettings|Shape")
    bool bOverrideShape = false;

    UPROPERTY(EditAnywhere, Category = "SpawnSettings|Shape", meta = (EditCondition = "bOverrideShape"))
    EAOEShape Shape = EAOEShape::Box;

    // ── 크기 설정 (Shape 오버라이드 시 활성화) ──
    UPROPERTY(EditAnywhere, Category = "SpawnSettings|Shape", meta = (EditCondition = "bOverrideShape && Shape == EAOEShape::Box", EditConditionHides))
    FVector BoxExtent = FVector(32.f, 32.f, 32.f);

    UPROPERTY(EditAnywhere, Category = "SpawnSettings|Shape", meta = (EditCondition = "bOverrideShape && (Shape == EAOEShape::Sphere || Shape == EAOEShape::Capsule)", EditConditionHides))
    float Radius = 32.f;

    UPROPERTY(EditAnywhere, Category = "SpawnSettings|Shape", meta = (EditCondition = "bOverrideShape && Shape == EAOEShape::Capsule", EditConditionHides))
    float CapsuleHalfHeight = 60.f;

    // [New] HitReactionTag 오버라이드
    UPROPERTY(EditAnywhere, Category = "SpawnSettings|Damage")
    bool bOverrideHitReactionTag = false;

    UPROPERTY(EditAnywhere, Category = "SpawnSettings|Damage", meta = (EditCondition = "bOverrideHitReactionTag"))
    FGameplayTag HitReactionTag;

    // 소환된 액터를 소켓에 계속 붙여둘지 여부
    UPROPERTY(EditAnywhere, Category = "SpawnSettings")
    bool bAttachToSocket = false;

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

    // 디버그 드로우 켜기
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebugDraw = false;

    /** 카메라 셰이크 클래스 (선택) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    TSubclassOf<UCameraShakeBase> CameraShakeClass;

    /** 셰이크 세기 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float CameraShakeScale = 1.f;
};
