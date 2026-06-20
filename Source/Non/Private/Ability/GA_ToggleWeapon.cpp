#include "Ability/GA_ToggleWeapon.h"

#include "GameplayTagContainer.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Character/NonCharacterBase.h"
#include "Components/MeshComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/InventoryItem.h"
#include "TimerManager.h"

UGA_ToggleWeapon::UGA_ToggleWeapon() {
  InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
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

  if (UEquipmentComponent *EquipComp =
          NonChar->FindComponentByClass<UEquipmentComponent>()) {
    if (UInventoryItem *MainItem =
            EquipComp->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain)) {
      if (AActor *SpawnedWeapon =
              EquipComp->GetEquippedActor(EEquipmentSlot::WeaponMain)) {
        const FName HandSocket = NonChar->GetHandSocketForItem(MainItem);
        if (SpawnedWeapon->GetAttachParentSocketName() != HandSocket) {
          bTargetArmed = true;
        }
      }
      else if (UMeshComponent *MainMesh =
                   EquipComp->GetVisualComponent(EEquipmentSlot::WeaponMain)) {
        const FName HandSocket = NonChar->GetHandSocketForItem(MainItem);
        if (MainMesh->GetAttachSocketName() != HandSocket) {
          bTargetArmed = true;
        }
      }
    }
  }

  UAnimMontage* SelectedMontage = nullptr;
  bool bIsMoving = false;

  if (UCharacterMovementComponent* MoveComp = NonChar->GetCharacterMovement()) {
    // 플레이어의 실제 조작 입력(가속도)이 있을 때만 이동 중으로 판별 (루트 모션 속도는 감지 배제)
    bIsMoving = (MoveComp->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER);
  }

  if (bTargetArmed) {
    SelectedMontage = bIsMoving ? DrawMontage_NoRoot : DrawMontage_Root;
  } else {
    SelectedMontage = bIsMoving ? SheatheMontage_NoRoot : SheatheMontage_Root;
  }

  if (!SelectedMontage) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    return;
  }

  // ── 3) 몽타주 재생 (GAS 리플리케이션을 위해 Task 사용) ──
  PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
      this, NAME_None, SelectedMontage, 1.0f, NAME_None,
      false, // bStopWhenAbilityEnds
      1.0f   // AnimRootMotionTranslationScale
  );

  if (PlayMontageTask) {
    PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);
    PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);
    PlayMontageTask->OnBlendOut.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);
    PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);

    ActiveMontage = SelectedMontage;
    PlayMontageTask->ReadyForActivation();

    // 제자리 루트 모션 몽타주 재생 시 회전을 제한하기 위한 태그 부여
    if (SelectedMontage == DrawMontage_Root || SelectedMontage == SheatheMontage_Root) {
      if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) {
        ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.ToggleWeapon.Root"), false));
      }
    }

    // 무기 토글이 활성화되어 있는 동안 캐릭터 이동 상태가 실시간 변화하는 것을 감시
    if (UWorld* World = GetWorld()) {
      World->GetTimerManager().SetTimer(
          MovementCheckTimerHandle,
          this,
          &UGA_ToggleWeapon::CheckCharacterMovement,
          0.03f, // 약 30fps 간격으로 체크
          true
      );
    }
  } else {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
  }
}

void UGA_ToggleWeapon::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled) {
  ClearMovementTimer();

  // 어빌리티 종료 시 회전 제한용 태그 정리
  if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) {
    ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.ToggleWeapon.Root"), false));
  }

  Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ToggleWeapon::OnMontageFinished() {
  ClearMovementTimer();
  ActiveMontage = nullptr;
  CachedNonChar = nullptr;

  EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_ToggleWeapon::CheckCharacterMovement() {
  if (!CachedNonChar.IsValid()) {
    ClearMovementTimer();
    return;
  }

  if (UCharacterMovementComponent* MoveComp = CachedNonChar->GetCharacterMovement()) {
    // 플레이어가 방향키를 입력하여 가속도가 발생했는지 실시간 체크
    bool bIsMovingNow = (MoveComp->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER);

    // 현재 재생 중인 몽타주가 NoRoot 버전들 중 하나인지 판단
    bool bCurrentlyNoRoot = (ActiveMontage == DrawMontage_NoRoot || ActiveMontage == SheatheMontage_NoRoot);

    // 1) 움직이기 시작했는데 현재 몽타주가 NoRoot가 아닌 경우 -> NoRoot로 전환
    if (bIsMovingNow && !bCurrentlyNoRoot) {
      SwitchMontage(true);
    }
    // 2) 멈췄는데 현재 몽타주가 NoRoot인 경우 -> Root로 전환
    else if (!bIsMovingNow && bCurrentlyNoRoot) {
      SwitchMontage(false);
    }
  }
}

void UGA_ToggleWeapon::SwitchMontage(bool bPlayNoRoot) {
  if (!ActiveMontage || !CachedNonChar.IsValid()) return;

  UAnimInstance* AnimInstance = CachedNonChar->GetMesh()->GetAnimInstance();
  if (!AnimInstance) return;

  // 현재 재생 중인 몽타주의 재생 진행 위치(Time Position) 계산
  float CurrentPosition = 0.f;
  FAnimMontageInstance* ActiveMontageInst = AnimInstance->GetActiveInstanceForMontage(ActiveMontage);
  if (ActiveMontageInst) {
    CurrentPosition = ActiveMontageInst->GetPosition();
  }

  // 타겟 몽타주 선택 (이동 여부에 맞춰 스왑)
  UAnimMontage* TargetMontage = nullptr;
  if (bTargetArmed) {
    TargetMontage = bPlayNoRoot ? DrawMontage_NoRoot : DrawMontage_Root;
  } else {
    TargetMontage = bPlayNoRoot ? SheatheMontage_NoRoot : SheatheMontage_Root;
  }

  if (!TargetMontage || TargetMontage == ActiveMontage) return;

  // 기존 몽타주 Task 정리
  if (PlayMontageTask) {
    PlayMontageTask->OnCompleted.RemoveAll(this);
    PlayMontageTask->OnInterrupted.RemoveAll(this);
    PlayMontageTask->OnBlendOut.RemoveAll(this);
    PlayMontageTask->OnCancelled.RemoveAll(this);
    PlayMontageTask->EndTask();
    PlayMontageTask = nullptr;
  }

  // 기존 몽타주 정지 (중첩 블렌딩 팝을 방지하기 위해 0.08초 동안 타이트하게 블렌드 아웃)
  AnimInstance->Montage_Stop(0.08f, ActiveMontage);

  // 동일한 시간대에서 시작하도록 새로운 PlayMontageTask 생성
  PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
      this, NAME_None, TargetMontage, 1.0f, NAME_None,
      false, // bStopWhenAbilityEnds
      1.0f   // AnimRootMotionTranslationScale
  );

  if (PlayMontageTask) {
    PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);
    PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);
    PlayMontageTask->OnBlendOut.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);
    PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);

    ActiveMontage = TargetMontage;
    PlayMontageTask->ReadyForActivation();

    // 제자리 루트 모션 몽타주 스왑 상태에 따른 태그 관리
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) {
      FGameplayTag RootTag = FGameplayTag::RequestGameplayTag(TEXT("State.ToggleWeapon.Root"), false);
      if (TargetMontage == DrawMontage_Root || TargetMontage == SheatheMontage_Root) {
        ASC->AddLooseGameplayTag(RootTag);
      } else {
        ASC->RemoveLooseGameplayTag(RootTag);
      }
    }

    // 몽타주가 활성화된 뒤 재생 시작 위치를 기존 진행 상황에 맞게 강제 설정
    AnimInstance->Montage_SetPosition(TargetMontage, CurrentPosition);
  } else {
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
  }
}

void UGA_ToggleWeapon::ClearMovementTimer() {
  if (UWorld* World = GetWorld()) {
    World->GetTimerManager().ClearTimer(MovementCheckTimerHandle);
  }
}
