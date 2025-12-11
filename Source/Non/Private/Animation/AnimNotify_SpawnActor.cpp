#include "Animation/AnimNotify_SpawnActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

#include "Kismet/GameplayStatics.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#include "Combat/DamageAOE.h" // 통합된 AOE 헤더
#include "Character/NonCharacterBase.h" 

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

    // 캐릭터에서 스킬 계수 가져오기 (없으면 1.0)
    float PowerScale = 1.f;
    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Owner))
    {
        PowerScale = NonChar->GetLastSkillDamageScale();
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[Notify] Owner=%s PowerScale=%.2f"),
        *Owner->GetName(),
        PowerScale);
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
            // BoxAOE면 Extent + 계수 설정
            if (ADamageAOE* AOE = Cast<ADamageAOE>(SpawnedActor))
            {
                // 1) Shape 오버라이드 적용
                if (bOverrideShape)
                {
                    AOE->Shape = Shape;

                    // 2) 크기 설정 (오버라이드 시에만 적용)
                    if (AOE->Shape == EAOEShape::Box)
                    {
                        AOE->BoxExtent = BoxExtent;
                    }
                    else if (AOE->Shape == EAOEShape::Sphere || AOE->Shape == EAOEShape::Capsule)
                    {
                        AOE->Radius = Radius;
                    }

                    if (AOE->Shape == EAOEShape::Capsule)
                    {
                        AOE->CapsuleHalfHeight = CapsuleHalfHeight;
                    }
                }

                // 디버그 설정 적용
                if (bEnableDebugDraw)
                {
                    AOE->bDebugDraw = true;
                }

                UE_LOG(LogTemp, Warning,
                    TEXT("[Notify] DamageAOE(%d) Spawned, Scale=%.2f"),
                    (uint8)AOE->Shape, PowerScale);

                AOE->SetPowerScale(PowerScale);

                // [New] HitReactionTag 오버라이드
                if (bOverrideHitReactionTag)
                {
                    AOE->HitReactionTag = HitReactionTag;
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
