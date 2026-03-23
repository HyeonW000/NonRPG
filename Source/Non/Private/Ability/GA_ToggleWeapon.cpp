#include "Ability/GA_ToggleWeapon.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Character/NonCharacterBase.h"
#include "Components/MeshComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/InventoryItem.h"


UGA_ToggleWeapon::UGA_ToggleWeapon() {
  InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

  // 태그는 BP에서 세팅해도 되고, 여기서 기본값을 줄 수도 있음
  // 예)
  // AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.ToggleWeapon")));
}

void UGA_ToggleWeapon::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo *ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData *TriggerEventData) {
  Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

  if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    return;
  }

  if (!ActorInfo || !ActorInfo->AvatarActor.IsValid()) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    return;
  }

  ACharacter *Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
  if (!Character) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    return;
  }

  ANonCharacterBase *NonChar = Cast<ANonCharacterBase>(Character);
  if (!NonChar) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    return;
  }

  CachedNonChar = NonChar;

  // ── 1) 이번 토글에서의 "목표 무장 상태" 계산 ──
  bTargetArmed = !NonChar->IsArmed();

  // [New] 방패 등 다른 요인으로 Armed 상태라도, 메인 무기가 아직 쉬스(집)에
  // 있다면 Draw 해야 함
  if (UEquipmentComponent *EquipComp =
          NonChar->FindComponentByClass<UEquipmentComponent>()) {
    if (UInventoryItem *MainItem =
            EquipComp->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain)) {
      // 메인 무기가 있는데, 아직 손에 없다면 -> Armed 목표 (꺼내기)
      // [New] 액터 베이스 무기 시스템 지원
      if (AActor *SpawnedWeapon =
              EquipComp->GetEquippedActor(EEquipmentSlot::WeaponMain)) {
        const FName HandSocket = NonChar->GetHandSocketForItem(MainItem);
        if (SpawnedWeapon->GetAttachParentSocketName() != HandSocket) {
          bTargetArmed = true;
        }
      }
      // 기존 메시 베이스 무기 시스템 호환 유지
      else if (UMeshComponent *MainMesh =
                   EquipComp->GetVisualComponent(EEquipmentSlot::WeaponMain)) {
        const FName HandSocket = NonChar->GetHandSocketForItem(MainItem);
        if (MainMesh->GetAttachSocketName() != HandSocket) {
          bTargetArmed = true;
        }
      }
    }
  }

  UAnimMontage *SelectedMontage = nullptr;

  if (bTargetArmed) {
    // 무장 쪽 몽타주 하나만 사용 (원하는 쪽 골라)
    SelectedMontage = DrawMontage; // 또는 DrawUpperBodyMontage
  } else {
    // 비무장 쪽 몽타주 하나만 사용
    SelectedMontage = SheatheMontage; // 또는 SheatheUpperBodyMontage
  }

  if (SelectedMontage) {

  } else {
  }

  // ── 3) 몽타주 재생 (GAS 리플리케이션을 위해 Task 사용) ──
  UAbilityTask_PlayMontageAndWait *Task =
      UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
          this, NAME_None, SelectedMontage, 1.0f, NAME_None,
          false, // bStopWhenAbilityEnds
          1.0f   // AnimRootMotionTranslationScale
      );

  if (Task) {
    // 몽타주가 끝나거나, 중단되거나, 취소되면 종료 처리
    Task->OnCompleted.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);
    Task->OnInterrupted.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);
    Task->OnBlendOut.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);
    Task->OnCancelled.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);

    ActiveMontage = SelectedMontage;
    Task->ReadyForActivation();
  } else {
    // 태스크 생성 실패 시 즉시 종료
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
  }
}

void UGA_ToggleWeapon::OnMontageFinished() {
  // 이제 무장 상태는 AnimNotify_SwapWeaponSocket에서 이미 처리함
  ActiveMontage = nullptr;
  CachedNonChar = nullptr;

  EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true,
             false);
}
