#include "Animation/Notifies/ANS_WeaponTrail.h"
#include "Character/NonCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "Equipment/WeaponBase.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"


UANS_WeaponTrail::UANS_WeaponTrail() { bIsNativeBranchingPoint = true; }

UNiagaraComponent *
UANS_WeaponTrail::FindTrailComponent(AActor *WeaponActor) const {
  if (!WeaponActor)
    return nullptr;

  TArray<UNiagaraComponent *> Comps;
  WeaponActor->GetComponents<UNiagaraComponent>(Comps);
  for (UNiagaraComponent *Comp : Comps) {
    if (Comp && Comp->ComponentTags.Contains(FName("WeaponTrail"))) {
      return Comp;
    }
  }
  return nullptr;
}

void UANS_WeaponTrail::NotifyBegin(
    USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation,
    float TotalDuration, const FAnimNotifyEventReference &EventReference) {
  Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

  if (!MeshComp || !TrailSystem)
    return;

  ANonCharacterBase *Char = Cast<ANonCharacterBase>(MeshComp->GetOwner());
  if (!Char)
    return;

  UEquipmentComponent *EqComp =
      Char->FindComponentByClass<UEquipmentComponent>();
  if (!EqComp)
    return;

  if (AActor *SpawnedActor =
          EqComp->GetEquippedActor(EEquipmentSlot::WeaponMain)) {
    if (AWeaponBase *Weapon = Cast<AWeaponBase>(SpawnedActor)) {
      if (Weapon->WeaponSkeletalMesh) {
        // 기존 이펙트 제거
        if (UNiagaraComponent *OldComp = FindTrailComponent(SpawnedActor)) {
          OldComp->Deactivate();
          OldComp->ComponentTags.Remove(FName("WeaponTrail"));
        }

        // 스켈레탈 메시의 `TraceStart` 뼈대(소켓) 기준으로 어태치
        UNiagaraComponent *SpawnedTrailComponent =
            UNiagaraFunctionLibrary::SpawnSystemAttached(
                TrailSystem, Weapon->WeaponSkeletalMesh, FName("TraceStart"), FVector::ZeroVector,
                FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);

        if (SpawnedTrailComponent) {
          SpawnedTrailComponent->ComponentTags.Add(FName("WeaponTrail"));
          SpawnedTrailComponent->SetVariablePosition(
              TEXT("TraceStart"), Weapon->WeaponSkeletalMesh->GetSocketLocation(FName("TraceStart")));
          SpawnedTrailComponent->SetVariablePosition(
              TEXT("TraceEnd"), Weapon->WeaponSkeletalMesh->GetSocketLocation(FName("TraceEnd")));
        }
      }
    }
  }
}

void UANS_WeaponTrail::NotifyTick(
    USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation,
    float FrameDeltaTime, const FAnimNotifyEventReference &EventReference) {
  Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

  ANonCharacterBase *Char = Cast<ANonCharacterBase>(MeshComp->GetOwner());
  if (!Char)
    return;

  UEquipmentComponent *EqComp =
      Char->FindComponentByClass<UEquipmentComponent>();
  if (!EqComp)
    return;

  if (AActor *SpawnedActor =
          EqComp->GetEquippedActor(EEquipmentSlot::WeaponMain)) {
    if (AWeaponBase *Weapon = Cast<AWeaponBase>(SpawnedActor)) {
      if (UNiagaraComponent *SpawnedTrailComponent = FindTrailComponent(SpawnedActor)) {
        if (Weapon->WeaponSkeletalMesh) {
          SpawnedTrailComponent->SetVariablePosition(
              TEXT("TraceStart"), Weapon->WeaponSkeletalMesh->GetSocketLocation(FName("TraceStart")));
          SpawnedTrailComponent->SetVariablePosition(
              TEXT("TraceEnd"), Weapon->WeaponSkeletalMesh->GetSocketLocation(FName("TraceEnd")));
        }
      }
    }
  }
}

void UANS_WeaponTrail::NotifyEnd(
    USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation,
    const FAnimNotifyEventReference &EventReference) {
  Super::NotifyEnd(MeshComp, Animation, EventReference);

  ANonCharacterBase *Char = Cast<ANonCharacterBase>(MeshComp->GetOwner());
  if (!Char)
    return;

  UEquipmentComponent *EqComp =
      Char->FindComponentByClass<UEquipmentComponent>();
  if (!EqComp)
    return;

  if (AActor *SpawnedActor =
          EqComp->GetEquippedActor(EEquipmentSlot::WeaponMain)) {
    if (UNiagaraComponent *SpawnedTrailComponent =
            FindTrailComponent(SpawnedActor)) {
      // 나이아가라 렌더링 중지 요청 -> 이펙트 내부의 Lifetime에 맞게 서서히
      // 죽음
      SpawnedTrailComponent->Deactivate();
      // 다음 타격에서 다시 찾히지 않게 태그만 먼저 뗀다
      SpawnedTrailComponent->ComponentTags.Remove(FName("WeaponTrail"));
    }
  }
}
