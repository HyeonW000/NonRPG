#include "Animation/AnimNotify_SpawnAOE.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_SpawnAOE::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (!MeshComp) return;
    UWorld* World = MeshComp->GetWorld();
    AActor* Owner = MeshComp->GetOwner();
    if (!World || !Owner) return;

    UClass* ClassToSpawn = (AOEClass != nullptr) ? *AOEClass : ADamageSphereAOE::StaticClass();

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SP.Owner = Owner;
    SP.Instigator = Cast<APawn>(Owner);

    // 소켓 위치 or 컴포넌트 위치
    const bool bUseSocket = (bAttachToSocket && SocketName != NAME_None && MeshComp->DoesSocketExist(SocketName));
    const FVector SpawnLoc = bUseSocket ? MeshComp->GetSocketLocation(SocketName)
        : MeshComp->GetComponentLocation();

    ADamageSphereAOE* AOE = World->SpawnActor<ADamageSphereAOE>(ClassToSpawn, SpawnLoc, FRotator::ZeroRotator, SP);
    if (!AOE) return;

    AOE->bDebugDraw = bDebugDraw;
    AOE->Configure(
        Radius, Damage, Duration, Interval,
        bSingleHitPerActor, Team,
        bAttachToSocket, SocketName, RelativeOffset, /*bServerOnly=*/true
    );

    // 비주얼 동기화 위해 소켓 부착(선택)
    if (bUseSocket)
    {
        AOE->AttachToComponent(MeshComp, FAttachmentTransformRules::KeepWorldTransform, SocketName);
    }
}
