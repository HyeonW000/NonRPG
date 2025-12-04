#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Kismet/KismetSystemLibrary.h" // EDrawDebugTrace::Type
#include "ANS_HitTrace.generated.h"

/**
 * 무기 소켓(시작/끝) 기준으로 구체 스윕(SphereTrace) 하여 타격 판정
 * - 서버 전용 판정(기본값): bServerOnly=true
 * - 한 NotifyState 동안 같은 액터는 한 번만 타격: bSingleHitPerActor=true
 * - 무기 메시 자동 탐색: PreferredComponentTag 우선, 없으면 소켓 존재하는 다른 SkeletalMeshComponent 검색
 * - 데미지 적용:
 *    * bUseGASDamage=true 이면 AEnemyCharacter::ApplyDamage 호출(없으면 무시)
 *    * false 이면 UGameplayStatics::ApplyPointDamage 사용(일반 데미지 파이프)
 */
UCLASS(meta = (DisplayName = "HitTrace: SphereTrace (Start->End)"))
class NON_API UANS_HitTrace : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    // --- Trace Settings ---
    UPROPERTY(EditAnywhere, Category = "HitTrace")
    FName StartSocket = TEXT("trace_start");

    UPROPERTY(EditAnywhere, Category = "HitTrace")
    FName EndSocket = TEXT("trace_end");

    UPROPERTY(EditAnywhere, Category = "HitTrace")
    float Radius = 12.f;

    UPROPERTY(EditAnywhere, Category = "HitTrace")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

    // 서버에서만 판정할지(권장)
    UPROPERTY(EditAnywhere, Category = "HitTrace")
    bool bServerOnly = true;

    // --- Damage Settings ---
    // GAS 데미지(AEnemyCharacter::ApplyDamage) 쓸지 여부
    UPROPERTY(EditAnywhere, Category = "HitTrace|Damage")
    bool bUseGASDamage = true;

    // 대미지 수치
    UPROPERTY(EditAnywhere, Category = "HitTrace|Damage", meta = (ClampMin = "0.0"))
    float Damage = 10.f;

    // 일반 데미지 경로 사용할 때 DamageType
    UPROPERTY(EditAnywhere, Category = "HitTrace|Damage", meta = (EditCondition = "!bUseGASDamage"))
    TSubclassOf<UDamageType> DamageType = UDamageType::StaticClass();

    // 한 NotifyState 동안 동일 액터는 한 번만 피격
    UPROPERTY(EditAnywhere, Category = "HitTrace|Damage")
    bool bSingleHitPerActor = true;

    // --- Debug ---
    UPROPERTY(EditAnywhere, Category = "HitTrace|Debug")
    TEnumAsByte<EDrawDebugTrace::Type> DebugDrawType = EDrawDebugTrace::None;

    UPROPERTY(EditAnywhere, Category = "HitTrace|Debug")
    FLinearColor DebugTraceColor = FLinearColor::Red;

    UPROPERTY(EditAnywhere, Category = "HitTrace|Debug")
    FLinearColor DebugHitColor = FLinearColor::Green;

    UPROPERTY(EditAnywhere, Category = "HitTrace|Debug")
    float DebugDrawTime = 0.f;

    // ---- Camera Shake ----
    UPROPERTY(EditAnywhere, Category = "HitTrace|CameraShake")
    TSubclassOf<UCameraShakeBase> CameraShakeClass;

    UPROPERTY(EditAnywhere, Category = "HitTrace|CameraShake", meta = (ClampMin = "0.0"))
    float CameraShakeScale = 1.0f;

    // --- Socket Owner Auto-Resolve ---
    // 1) 우선 MeshComp에서 찾고, 없으면 2) Owner의 모든 SkeletalMeshComponent에서
    // 두 소켓을 모두 가진 컴포넌트를 탐색
    UPROPERTY(EditAnywhere, Category = "HitTrace|SocketOwner")
    bool bAutoFindSocketOwnerOnOwner = true;

    // 특정 태그가 붙은 컴포넌트를 우선 탐색하고 싶으면 설정(예: "WeaponMesh")
    UPROPERTY(EditAnywhere, Category = "HitTrace|SocketOwner")
    FName PreferredComponentTag;

private:
    // 찾은 소켓 소유 컴포넌트 캐시
    TWeakObjectPtr<USceneComponent> SocketOwnerComp;

    // 중복 타격 방지용
    TSet<TWeakObjectPtr<AActor>> HitActors;

    // 소켓 소유 컴포넌트 찾기
    USceneComponent* ResolveSocketOwner(USkeletalMeshComponent* MeshComp) const;

public:
    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        float TotalDuration,
        const FAnimNotifyEventReference& EventReference) override;

    virtual void NotifyTick(USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        float FrameDeltaTime,
        const FAnimNotifyEventReference& EventReference) override;

    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp,
        UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
};
