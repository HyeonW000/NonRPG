#include "Animation/AnimNotify_SpawnActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

#include "Kismet/GameplayStatics.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#include "Combat/DamageBoxAOE.h" // BoxAOE 설정을 위해 포함
#include "Combat/DamageSphereAOE.h"

void UAnimNotify_SpawnActor::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (!MeshComp || !ActorClass) return;

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner) return;

    UWorld* World = MeshComp->GetWorld();
    if (!World) return;

    // 스폰 위치 계산
    FVector SpawnLoc;
    FRotator SpawnRot;

    if (SocketName.IsNone())
    {
        SpawnLoc = Owner->GetActorLocation();
        SpawnRot = Owner->GetActorRotation();
    }
    else if (MeshComp->DoesSocketExist(SocketName))
    {
        SpawnLoc = MeshComp->GetSocketLocation(SocketName);
        SpawnRot = MeshComp->GetSocketRotation(SocketName);
    }
    else
    {
        SpawnLoc = MeshComp->GetComponentLocation();
        SpawnRot = MeshComp->GetComponentRotation();
    }

    FTransform BaseTrans(SpawnRot, SpawnLoc);
    FTransform OffsetTrans(RotationOffset, LocationOffset);
    FTransform FinalTrans = OffsetTrans * BaseTrans;

    // ──────────────
    // 1) 서버에서만 AOE 스폰
    // ──────────────
    if (Owner->HasAuthority())
    {
        AActor* SpawnedActor = World->SpawnActorDeferred<AActor>(
            ActorClass,
            FinalTrans,
            Owner,
            Cast<APawn>(Owner),
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        );

        if (SpawnedActor)
        {
            // BoxAOE면 Extent 덮어쓰기
            if (bOverrideBoxExtent)
            {
                if (ADamageBoxAOE* BoxAOE = Cast<ADamageBoxAOE>(SpawnedActor))
                {
                    BoxAOE->BoxExtent = BoxExtent;
                }
            }

            UGameplayStatics::FinishSpawningActor(SpawnedActor, FinalTrans);
        }
    }

    // ──────────────
    // 2) 로컬 플레이어 카메라 셰이크
    // ──────────────
    if (CameraShakeClass)
    {
        if (APawn* PawnOwner = Cast<APawn>(Owner))
        {
            if (APlayerController* PC = Cast<APlayerController>(PawnOwner->GetController()))
            {
                if (PC->IsLocalController())
                {
                    PC->ClientStartCameraShake(CameraShakeClass, CameraShakeScale);
                }
            }
        }
    }
}
