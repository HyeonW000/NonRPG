#include "Effects/JustDodgeAfterimage.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AJustDodgeAfterimage::AJustDodgeAfterimage()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = false;
    SetCanBeDamaged(false);

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

void AJustDodgeAfterimage::InitFromActor(AActor* Source, UMaterialInterface* InMaterial, const FLinearColor& InColor, float InLifetime)
{
    Lifetime = FMath::Max(0.05f, InLifetime);
    Age = 0.f;

    if (!Source || !InMaterial)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Afterimage] skipped: Source=%s Material=%s"),
               *GetNameSafe(Source), *GetNameSafe(InMaterial));
        Destroy();
        return;
    }

    MID = UMaterialInstanceDynamic::Create(InMaterial, this);
    if (MID)
    {
        MID->SetVectorParameterValue(ColorParamName, InColor);
        MID->SetScalarParameterValue(OpacityParamName, StartOpacity);
    }

    // 캐릭터 본체의 메시 + 붙어 있는 액터(무기 등)의 메시
    TArray<AActor*> Sources;
    Sources.Add(Source);
    TArray<AActor*> Attached;
    Source->GetAttachedActors(Attached, true, true);
    Sources.Append(Attached);

    for (AActor* A : Sources)
    {
        if (!A || A->IsHidden())
            continue;

        TArray<UMeshComponent*> Meshes;
        A->GetComponents<UMeshComponent>(Meshes);
        for (UMeshComponent* M : Meshes)
        {
            AddMeshCopy(M);
        }
    }

    UE_LOG(LogTemp, Verbose, TEXT("[Afterimage] %s: copied %d mesh(es), lifetime %.2fs"),
           *GetNameSafe(Source), CopiedMeshes.Num(), Lifetime);

    if (CopiedMeshes.Num() == 0)
    {
        Destroy();
    }
}

void AJustDodgeAfterimage::AddMeshCopy(UMeshComponent* SourceComp)
{
    if (!SourceComp || !SourceComp->IsRegistered() || !SourceComp->IsVisible() || SourceComp->bHiddenInGame)
        return;

    UMeshComponent* Copy = nullptr;

    if (USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(SourceComp))
    {
        if (!Skel->GetSkinnedAsset())
            return;

        UPoseableMeshComponent* Poseable = NewObject<UPoseableMeshComponent>(this);
        Poseable->SetupAttachment(GetRootComponent());
        Poseable->SetSkinnedAssetAndUpdate(Skel->GetSkinnedAsset());
        Poseable->RegisterComponent();
        Poseable->SetWorldTransform(Skel->GetComponentTransform());

        // 리더 포즈를 따르는 컴포넌트(머리카락/장비 등)는 자기 본 데이터가 없으므로 리더 쪽 자세를 복사
        USkeletalMeshComponent* PoseSource = Skel;
        if (USkeletalMeshComponent* Leader = Cast<USkeletalMeshComponent>(Skel->LeaderPoseComponent.Get()))
        {
            PoseSource = Leader;
        }
        Poseable->CopyPoseFromSkeletalComponent(PoseSource);
        Copy = Poseable;
    }
    else if (UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(SourceComp))
    {
        if (!Static->GetStaticMesh())
            return;

        UStaticMeshComponent* StaticCopy = NewObject<UStaticMeshComponent>(this);
        StaticCopy->SetupAttachment(GetRootComponent());
        StaticCopy->SetStaticMesh(Static->GetStaticMesh());
        StaticCopy->RegisterComponent();
        StaticCopy->SetWorldTransform(Static->GetComponentTransform());
        Copy = StaticCopy;
    }

    if (!Copy)
        return;

    Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Copy->SetCastShadow(false);
    Copy->SetReceivesDecals(false);
    for (int32 i = 0; i < Copy->GetNumMaterials(); ++i)
    {
        Copy->SetMaterial(i, MID);
    }
    CopiedMeshes.Add(Copy);
}

void AJustDodgeAfterimage::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    Age += DeltaSeconds;
    const float Alpha = FMath::Clamp(1.f - Age / Lifetime, 0.f, 1.f);
    if (MID)
    {
        MID->SetScalarParameterValue(OpacityParamName, StartOpacity * Alpha);
    }

    if (Age >= Lifetime)
    {
        Destroy();
    }
}
