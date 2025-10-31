#include "Animation/ANS_HitTrace.h"
#include "Character/NonCharacterBase.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraShakeBase.h"

#include "AI/EnemyCharacter.h" // bUseGASDamage=true일 때 캐스팅하여 ApplyDamage 호출

// 소켓 소유 컴포넌트 찾기
USceneComponent* UANS_HitTrace::ResolveSocketOwner(USkeletalMeshComponent* MeshComp) const
{
    if (!MeshComp) return nullptr;

    // 1) 현재 메시에 소켓 둘 다 있으면 그대로 사용
    if (MeshComp->DoesSocketExist(StartSocket) && MeshComp->DoesSocketExist(EndSocket))
    {
        return MeshComp;
    }

    if (!bAutoFindSocketOwnerOnOwner) return nullptr;

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner) return nullptr;

    // Owner의 모든 SkeletalMeshComponent 탐색
    TArray<USkeletalMeshComponent*> SkelComps;
    Owner->GetComponents<USkeletalMeshComponent>(SkelComps);

    // (a) 태그 우선
    if (!PreferredComponentTag.IsNone())
    {
        for (USkeletalMeshComponent* C : SkelComps)
        {
            if (C && C->ComponentHasTag(PreferredComponentTag)
                && C->DoesSocketExist(StartSocket)
                && C->DoesSocketExist(EndSocket))
            {
                return C;
            }
        }
    }

    // (b) 그 외 컴포넌트에서 소켓 둘 다 있는 것
    for (USkeletalMeshComponent* C : SkelComps)
    {
        if (!C) continue;
        if (C->DoesSocketExist(StartSocket) && C->DoesSocketExist(EndSocket))
        {
            return C;
        }
    }

    return nullptr;
}

void UANS_HitTrace::NotifyBegin(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    float TotalDuration,
    const FAnimNotifyEventReference& EventReference)
{
    HitActors.Reset();
    SocketOwnerComp = ResolveSocketOwner(MeshComp);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    if (!SocketOwnerComp.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[HitTrace] Sockets not found. Start=%s End=%s"),
            *StartSocket.ToString(), *EndSocket.ToString());
    }
#endif
}

void UANS_HitTrace::NotifyEnd(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    HitActors.Reset();
    SocketOwnerComp.Reset();
}

void UANS_HitTrace::NotifyTick(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    float FrameDeltaTime,
    const FAnimNotifyEventReference& EventReference)
{
    if (!MeshComp) return;

    AActor* Owner = MeshComp->GetOwner();
    UWorld* World = MeshComp->GetWorld();
    if (!Owner || !World) return;

    if (bServerOnly && !Owner->HasAuthority())
    {
        return;
    }

    USceneComponent* Comp = SocketOwnerComp.IsValid() ? SocketOwnerComp.Get() : MeshComp;
    if (!Comp) return;

    if (!(Comp->DoesSocketExist(StartSocket) && Comp->DoesSocketExist(EndSocket)))
    {
        USceneComponent* ReResolved = ResolveSocketOwner(MeshComp);
        if (!ReResolved) return;
        Comp = ReResolved;
        SocketOwnerComp = ReResolved;
    }

    const FVector Start = Comp->GetSocketLocation(StartSocket);
    const FVector End = Comp->GetSocketLocation(EndSocket);

    // Trace
    TArray<FHitResult> Hits;
    const ETraceTypeQuery TraceType = UEngineTypes::ConvertToTraceType(TraceChannel);

    TArray<AActor*> Ignore;
    Ignore.Add(Owner);

    UKismetSystemLibrary::SphereTraceMulti(
        World, Start, End, Radius, TraceType, false, Ignore,
        DebugDrawType, Hits, true, DebugTraceColor, DebugHitColor, DebugDrawTime
    );

    APawn* InstigatorPawn = Cast<APawn>(Owner);
    bool bShookOnce = false;

    for (const FHitResult& H : Hits)
    {
        AActor* Other = H.GetActor();
        if (!Other || Other == Owner) continue;

        if (bSingleHitPerActor && HitActors.Contains(Other))
        {
            continue;
        }

        // ── 데미지 적용 ──
        if (bUseGASDamage)
        {
            //  Enemy에 한정하지 말고 공통 베이스로!
            if (AEnemyCharacter* Victim = Cast<AEnemyCharacter>(Other))
            {
                FVector HitLoc = Victim->GetActorLocation();
                if (!H.ImpactPoint.IsNearlyZero())
                {
                    HitLoc = H.ImpactPoint;
                }
                else if (!H.Location.IsNearlyZero())
                {
                    HitLoc = H.Location;
                }

                Victim->ApplyDamageAt(Damage, Owner, HitLoc);
            }
            else
            {
                const FVector Dir = (End - Start).GetSafeNormal();
                UGameplayStatics::ApplyPointDamage(
                    Other, Damage, Dir, H,
                    InstigatorPawn ? InstigatorPawn->GetController() : nullptr,
                    Owner, DamageType
                );
            }
        }
        else
        {
            const FVector Dir = (End - Start).GetSafeNormal();
            UGameplayStatics::ApplyPointDamage(
                Other, Damage, Dir, H,
                Owner->GetInstigatorController(), Owner, DamageType
            );
        }

        // 카메라 셰이크(선택)
        if (CameraShakeClass && InstigatorPawn && !bShookOnce)
        {
            if (APlayerController* PC = Cast<APlayerController>(InstigatorPawn->GetController()))
            {
                PC->ClientStartCameraShake(CameraShakeClass, CameraShakeScale);
                bShookOnce = true;
            }
        }

        HitActors.Add(Other);
    }
}
