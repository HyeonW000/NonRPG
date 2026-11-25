#include "Ability/GA_ToggleWeapon.h"
#include "Core/NonGameplayTags.h"

#include "GameplayTagContainer.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/NonCharacterBase.h"
#include "Equipment/EquipmentComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UGA_ToggleWeapon::UGA_ToggleWeapon() {
  InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

  // 발도/납도 중 자기 자신이 중복 실행되어 몽타주를 끊지 않도록 방어
  FGameplayTag ToggleTag = NonTags::Ability_ToggleWeapon.GetTag();
  if (ToggleTag.IsValid()) {
    BlockAbilitiesWithTag.AddTag(ToggleTag);
  }
}

void UGA_ToggleWeapon::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo *ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData *TriggerEventData) {
  Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

  if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    return;
  }

  ANonCharacterBase *NonChar = (ActorInfo && ActorInfo->AvatarActor.IsValid())
                                   ? Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get())
                                   : nullptr;
  if (!NonChar) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    return;
  }

  CachedNonChar = NonChar;
  bRootMotionDisabledForMove = false;
  NonChar->SetDrawSheatheMoving(false);

  // ── 1) 이번 토글의 목표 무장 상태와 몽타주 ──
  bTargetArmed = !NonChar->IsArmed();
  UAnimMontage *SelectedMontage = bTargetArmed ? DrawMontage_Root : SheatheMontage_Root;
  if (!SelectedMontage) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    return;
  }

  bool bIsMoving = false;
  if (UCharacterMovementComponent *MoveComp = NonChar->GetCharacterMovement()) {
    bIsMoving = (NonChar->GetLastMovementInputVector().SizeSquared() > KINDA_SMALL_NUMBER) ||
                (MoveComp->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER);
  }

  // ── 2) 몽타주 재생 (OnBlendOut 조기 종료 없이 OnCompleted에서만 정상 종료하여 노티파이 스킵 방지) ──
  PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
      this, NAME_None, SelectedMontage, 1.0f, NAME_None,
      false, // bStopWhenAbilityEnds
      1.0f   // AnimRootMotionTranslationScale
  );
  if (!PlayMontageTask) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    return;
  }

  PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);
  PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_ToggleWeapon::OnMontageInterrupted);
  PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_ToggleWeapon::OnMontageInterrupted);

  ActiveMontage = SelectedMontage;
  PlayMontageTask->ReadyForActivation();

  // ── 3) 제자리면 루트 이동 + 회전 잠금, 이동 중이면 루트 이동 끔 (하체는 애님 BP가 이동 애니메이션과 섞음) ──
  if (bIsMoving) {
    DisableRootMotionForMove();
  } else if (UAbilitySystemComponent *ASC = GetAbilitySystemComponentFromActorInfo()) {
    const FGameplayTag RootLockTag = NonTags::State_ToggleWeapon_Root.GetTag();
    if (RootLockTag.IsValid()) ASC->SetLooseGameplayTagCount(RootLockTag, 1);
  }

  // 재생 중 이동 시작 감지
  if (UWorld *World = GetWorld()) {
    World->GetTimerManager().SetTimer(MovementCheckTimerHandle, this,
                                      &UGA_ToggleWeapon::CheckCharacterMovement, 0.03f, true);
  }
}

void UGA_ToggleWeapon::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled) {
  ClearMovementTimer();

  ANonCharacterBase* NonChar = CachedNonChar.Get();
  if (!NonChar && ActorInfo && ActorInfo->AvatarActor.IsValid()) {
    NonChar = Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get());
  }

  if (NonChar) {
    NonChar->SetDrawSheatheMoving(false);
  }

  // 회전 잠금 태그 정리
  if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) {
    const FGameplayTag RootLockTag = NonTags::State_ToggleWeapon_Root.GetTag();
    if (RootLockTag.IsValid()) {
      ASC->SetLooseGameplayTagCount(RootLockTag, 0);
    }
  }

  // 멀티플레이에서는 클라이언트가 끝낸 경우 서버에서는 OnMontageFinished 없이 EndAbility만 불리므로
  // 여기서 무장 상태·소켓·태그를 맞춰야 서버와 클라이언트 양쪽이 같아짐
  if (!bWasCancelled) {
    ApplyTargetWeaponState();
  } else if (NonChar) {
    if (UEquipmentComponent* EquipComp = NonChar->FindComponentByClass<UEquipmentComponent>()) {
      EquipComp->UpdateWeaponTags(NonChar->IsArmed());
    }
  }

  ActiveMontage = nullptr;
  CachedNonChar = nullptr;

  Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ToggleWeapon::ApplyTargetWeaponState() {
  ANonCharacterBase* NonChar = CachedNonChar.Get();
  if (!NonChar && CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid()) {
    NonChar = Cast<ANonCharacterBase>(CurrentActorInfo->AvatarActor.Get());
  }
  if (!NonChar) return;

  // 노티파이가 스킵되었더라도 무기 소켓과 무장 상태·스탠스를 목표대로 맞춤
  NonChar->ReattachMainWeaponForArmedState(bTargetArmed);
  if (NonChar->IsArmed() != bTargetArmed) {
    NonChar->SetArmed(bTargetArmed);
  } else {
    NonChar->RefreshWeaponStance();
  }

  if (UEquipmentComponent* EquipComp = NonChar->FindComponentByClass<UEquipmentComponent>()) {
    EquipComp->UpdateWeaponTags(NonChar->IsArmed());
  }
}

void UGA_ToggleWeapon::OnMontageFinished() {
  ClearMovementTimer();
  EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_ToggleWeapon::OnMontageInterrupted() {
  ClearMovementTimer();

  // 끊긴 시점에 이미 노티파이로 무장 상태가 바뀌었다면 취소가 아니라 정상 완료로 처리
  const bool bAlreadySwapped = (CachedNonChar.IsValid() && CachedNonChar->IsArmed() == bTargetArmed);
  EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bAlreadySwapped);
}

void UGA_ToggleWeapon::CheckCharacterMovement() {
  if (!CachedNonChar.IsValid()) {
    ClearMovementTimer();
    return;
  }
  if (bRootMotionDisabledForMove) {
    return; // 이미 껐으면 다시 멈춰도 켜지 않음
  }

  if (UCharacterMovementComponent* MoveComp = CachedNonChar->GetCharacterMovement()) {
    // 루트 모션으로 생기는 속도와 구분하기 위해 이동 입력(WASD)과 가속도로 판단
    const bool bHasInput = (CachedNonChar->GetLastMovementInputVector().SizeSquared() > KINDA_SMALL_NUMBER);
    const bool bHasAccel = (MoveComp->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER);
    if (bHasInput || bHasAccel) {
      DisableRootMotionForMove();
    }
  }
}

void UGA_ToggleWeapon::DisableRootMotionForMove() {
  if (bRootMotionDisabledForMove || !ActiveMontage || !CachedNonChar.IsValid()) {
    return;
  }
  USkeletalMeshComponent* Mesh = CachedNonChar->GetMesh();
  UAnimInstance* AnimInst = Mesh ? Mesh->GetAnimInstance() : nullptr;
  if (!AnimInst) {
    return;
  }
  if (FAnimMontageInstance* Inst = AnimInst->GetActiveInstanceForMontage(ActiveMontage)) {
    // 남은 루트 이동은 버리고 입력대로 이동 (몽타주 자체는 계속 재생, 상체는 그대로)
    Inst->PushDisableRootMotion();
    bRootMotionDisabledForMove = true;
  }
  // 다른 플레이어 화면이 하체를 걷기로 섞도록 서버 판단을 복제
  CachedNonChar->SetDrawSheatheMoving(true);
  // 다른 플레이어 화면은 이 슬롯 몽타주의 루트 이동을 시작부터 꺼 두므로 별도 알림 불필요
  // (ANonCharacterBase::HandleMontageStartedForProxyRootMotion)
  if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) {
    const FGameplayTag RootLockTag = NonTags::State_ToggleWeapon_Root.GetTag();
    if (RootLockTag.IsValid()) ASC->SetLooseGameplayTagCount(RootLockTag, 0);
  }
}

void UGA_ToggleWeapon::ClearMovementTimer() {
  if (UWorld* World = GetWorld()) {
    World->GetTimerManager().ClearTimer(MovementCheckTimerHandle);
  }
}
