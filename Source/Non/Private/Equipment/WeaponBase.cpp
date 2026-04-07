#include "Equipment/WeaponBase.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"


AWeaponBase::AWeaponBase() {
  PrimaryActorTick.bCanEverTick = false;

  Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
  SetRootComponent(Root);

  WeaponSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(
      TEXT("WeaponSkeletalMesh"));
  WeaponSkeletalMesh->SetupAttachment(Root);
  WeaponSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  WeaponSkeletalMesh->SetGenerateOverlapEvents(false);
  WeaponSkeletalMesh->SetReceivesDecals(false); // 데칼 방지

  WeaponStaticMesh =
      CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponStaticMesh"));
  WeaponStaticMesh->SetupAttachment(Root);
  WeaponStaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  WeaponStaticMesh->SetGenerateOverlapEvents(false);
  WeaponStaticMesh->SetReceivesDecals(false); // 데칼 방지
}

void AWeaponBase::BeginPlay() { Super::BeginPlay(); }

void AWeaponBase::Tick(float DeltaTime) { Super::Tick(DeltaTime); }
