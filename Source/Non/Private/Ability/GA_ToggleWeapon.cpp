#include "Ability/GA_ToggleWeapon.h"
#include "Core/NonGameplayTags.h"

#include "GameplayTagContainer.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_SwapWeaponSocket.h"
#include "Character/NonCharacterBase.h"
#include "Components/MeshComponent.h"
#include "Engine/Engine.h"
#include "Equipment/EquipmentComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/InventoryItem.h"
#include "TimerManager.h"

namespace
{
  // 몽타주 내 UAnimNotify_SwapWeaponSocket 노티파이의 트리거 시간(초)을 조회 (없으면 -1.f 반환)
  float FindSwapNotifyTriggerTime(const UAnimMontage* Montage)
  {
    if (!Montage) return -1.f;
    for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
    {
      if (NotifyEvent.Notify && NotifyEvent.Notify->IsA<UAnimNotify_SwapWeaponSocket>())
      {
        return NotifyEvent.GetTriggerTime();
      }
    }
    return -1.f;
  }
}

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

  bool bIsMoving = false;
  if (UCharacterMovementComponent* MoveComp = NonChar->GetCharacterMovement()) {
    bIsMoving = (NonChar->GetLastMovementInputVector().SizeSquared() > KINDA_SMALL_NUMBER) ||
                (MoveComp->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER);
  }

  // ── 2) 정지 시 _Root(DefaultSlot, 전신), 이동 시 _NoRoot(UpperBody 슬롯, 상체) 선택 (미지정 시 상호 폴백) ──
  UAnimMontage* SelectedMontage = nullptr;
  if (bTargetArmed) {
    SelectedMontage = bIsMoving
        ? (DrawMontage_NoRoot ? DrawMontage_NoRoot : DrawMontage_Root)
        : (DrawMontage_Root ? DrawMontage_Root : DrawMontage_NoRoot);
  } else {
    SelectedMontage = bIsMoving
        ? (SheatheMontage_NoRoot ? SheatheMontage_NoRoot : SheatheMontage_Root)
        : (SheatheMontage_Root ? SheatheMontage_Root : SheatheMontage_NoRoot);
  }

  if (!SelectedMontage) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    return;
  }

  // ── 3) 몽타주 재생 (OnBlendOut 조기 종료를 걸지 않고 OnCompleted에서만 정상 종료하여 노티파이 스킵 방지) ──
  PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
      this, NAME_None, SelectedMontage, 1.0f, NAME_None,
      false, // bStopWhenAbilityEnds
      1.0f   // AnimRootMotionTranslationScale
  );

  if (PlayMontageTask) {
    PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);
    PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_ToggleWeapon::OnMontageInterrupted);
    PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_ToggleWeapon::OnMontageInterrupted);

    ActiveMontage = SelectedMontage;
    PlayMontageTask->ReadyForActivation();

    // 제자리 루트 모션(_Root, DefaultSlot) 재생 시 풀바디 강제 및 회전 제한 태그 부여
    const bool bIsRootMontage = (SelectedMontage == DrawMontage_Root || SelectedMontage == SheatheMontage_Root);
    const FGameplayTag RootLockTag = NonTags::State_ToggleWeapon_Root.GetTag();
    if (bIsRootMontage && !bIsMoving) {
      if (!bHasRequestedFullBody) {
        NonChar->SetForceFullBody(true);
        bHasRequestedFullBody = true;
      }
      if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) {
        if (RootLockTag.IsValid()) ASC->SetLooseGameplayTagCount(RootLockTag, 1);
      }
    } else {
      if (bHasRequestedFullBody) {
        NonChar->SetForceFullBody(false);
        bHasRequestedFullBody = false;
      }
      if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) {
        if (RootLockTag.IsValid()) ASC->SetLooseGameplayTagCount(RootLockTag, 0);
      }
    }

    // 무기 토글이 활성화되어 있는 동안 캐릭터 이동 상태가 실시간 변화하는 것을 감시
    if (UWorld* World = GetWorld()) {
      World->GetTimerManager().SetTimer(
          MovementCheckTimerHandle,
          this,
          &UGA_ToggleWeapon::CheckCharacterMovement,
          0.03f,
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

  ANonCharacterBase* NonChar = CachedNonChar.Get();
  if (!NonChar && ActorInfo && ActorInfo->AvatarActor.IsValid()) {
    NonChar = Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get());
  }

  // 풀바디 상태 안전 해제
  if (bHasRequestedFullBody) {
    if (NonChar) {
      NonChar->SetForceFullBody(false);
    }
    bHasRequestedFullBody = false;
  }

  // 어빌리티 종료 시 회전 제한용 태그 정리
  if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) {
    const FGameplayTag RootLockTag = NonTags::State_ToggleWeapon_Root.GetTag();
    if (RootLockTag.IsValid()) {
      ASC->SetLooseGameplayTagCount(RootLockTag, 0);
    }
  }

  // ★ 핵심: 멀티플레이(Play As Client)에서는 클라이언트가 EndAbility를 호출하면 서버에는 OnMontageFinished가 아닌
  // ServerEndAbility -> EndAbility가 직접 호출됩니다!
  // 따라서 EndAbility 내부에서 무장 상태·소켓·태그 동기화를 수행해야 서버와 클라이언트 양쪽 모두 100% 보장됩니다!
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

  UEquipmentComponent* EquipComp = NonChar->FindComponentByClass<UEquipmentComponent>();
  if (EquipComp) {
    if (UInventoryItem* MainItem = EquipComp->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain)) {
      FName DesiredSocket = NAME_None;
      if (bTargetArmed) {
        DesiredSocket = NonChar->GetHandSocketForItem(MainItem);
      } else {
        DesiredSocket = EquipComp->GetHomeSocketForSlot(EEquipmentSlot::WeaponMain);
        if (DesiredSocket == NAME_None) {
          DesiredSocket = NonChar->GetHomeFallbackSocketForItem(MainItem);
        }
      }

      if (DesiredSocket != NAME_None) {
        bool bNeedReattach = false;
        if (AActor* SpawnedWeapon = EquipComp->GetEquippedActor(EEquipmentSlot::WeaponMain)) {
          if (SpawnedWeapon->GetAttachParentSocketName() != DesiredSocket) {
            bNeedReattach = true;
          }
        } else if (UMeshComponent* MainMesh = EquipComp->GetVisualComponent(EEquipmentSlot::WeaponMain)) {
          if (MainMesh->GetAttachSocketName() != DesiredSocket) {
            bNeedReattach = true;
          }
        }

        if (bNeedReattach) {
          EquipComp->ReattachSlotToSocket(EEquipmentSlot::WeaponMain, DesiredSocket, FTransform::Identity);
        }
      }
    }
  }

  // 노티파이가 스킵되었더라도 무장 상태와 스탠스를 100% 일치시킴
  if (NonChar->IsArmed() != bTargetArmed) {
    NonChar->SetArmed(bTargetArmed);
  } else {
    NonChar->RefreshWeaponStance();
  }

  // 무기 태그(State.Armed.*)도 목표 무장 상태에 맞춰 100% 보장
  if (EquipComp) {
    EquipComp->UpdateWeaponTags(NonChar->IsArmed());
  }
}

void UGA_ToggleWeapon::OnMontageFinished() {
  ClearMovementTimer();
  EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_ToggleWeapon::OnMontageInterrupted() {
  // ★ 핵심 방어: Root(DefaultSlot) <-> NoRoot(UpperBody 슬롯) 스왑이나 멀티플레이 리플리케이션 과정에서
  // 이전 몽타주의 OnInterrupted가 호출된 경우, 다른 발도/납도 몽타주가 재생 중이면 절대 어빌리티를 취소하지 않음!
  if (CachedNonChar.IsValid() && CachedNonChar->GetMesh()) {
    if (UAnimInstance* AnimInst = CachedNonChar->GetMesh()->GetAnimInstance()) {
      UAnimMontage* RootMon = bTargetArmed ? DrawMontage_Root : SheatheMontage_Root;
      UAnimMontage* NoRootMon = bTargetArmed ? DrawMontage_NoRoot : SheatheMontage_NoRoot;
      if ((RootMon && AnimInst->Montage_IsPlaying(RootMon)) ||
          (NoRootMon && AnimInst->Montage_IsPlaying(NoRootMon))) {
        return;
      }
    }
  }

  ClearMovementTimer();

  // 만약 인터럽트 시점에 이미 노티파이가 실행되어 소켓/bArmed가 바뀐 뒤라면 취소로 처리하지 않고 정상 완료 동기화
  bool bAlreadySwapped = (CachedNonChar.IsValid() && CachedNonChar->IsArmed() == bTargetArmed);
  EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bAlreadySwapped);
}

void UGA_ToggleWeapon::CheckCharacterMovement() {
  if (!CachedNonChar.IsValid()) {
    ClearMovementTimer();
    return;
  }

  if (UCharacterMovementComponent* MoveComp = CachedNonChar->GetCharacterMovement()) {
    // 루트 모션 이동 속도와 유저 조작을 구분하기 위해 유저 입력(WASD) 및 가속도를 우선 검사
    const bool bHasInput = (CachedNonChar->GetLastMovementInputVector().SizeSquared() > KINDA_SMALL_NUMBER);
    const bool bHasAccel = (MoveComp->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER);
    bool bIsMovingNow = bHasInput || bHasAccel;

    bool bCurrentlyNoRoot = (ActiveMontage == DrawMontage_NoRoot || ActiveMontage == SheatheMontage_NoRoot);

    // 1) 움직이기 시작했는데 현재 몽타주가 NoRoot(UpperBody 슬롯)가 아닌 경우 -> NoRoot로 전환
    if (bIsMovingNow && !bCurrentlyNoRoot) {
      SwitchMontage(true);
    }
    // 2) 멈췄는데 현재 몽타주가 NoRoot(UpperBody 슬롯)인 경우 -> Root(DefaultSlot)로 전환
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

  const UAnimMontage* OldMontage = ActiveMontage;
  const float OldLength = FMath::Max(0.01f, OldMontage->GetPlayLength());
  const float TargetLength = FMath::Max(0.01f, TargetMontage->GetPlayLength());

  const float ProgressRatio = FMath::Clamp(CurrentPosition / OldLength, 0.f, 0.95f);
  const float NewPosition = ProgressRatio * TargetLength;

  // 스왑 시점에 이미 노티파이 시간을 지났다면 즉시 무기 소켓·무장 상태·태그를 적용하여 누락 원천 차단
  const float OldNotifyTime = FindSwapNotifyTriggerTime(OldMontage);
  const float TargetNotifyTime = FindSwapNotifyTriggerTime(TargetMontage);
  const bool bPassedOldNotify = (OldNotifyTime >= 0.f && CurrentPosition >= OldNotifyTime - 0.03f);
  const bool bPassedTargetNotify = (TargetNotifyTime >= 0.f && NewPosition >= TargetNotifyTime - 0.03f);

  if ((bPassedOldNotify || bPassedTargetNotify) && CachedNonChar->IsArmed() != bTargetArmed) {
    ApplyTargetWeaponState();
  }

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
  AnimInstance->Montage_Stop(0.08f, OldMontage);

  // 새로운 PlayMontageTask 생성
  PlayMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
      this, NAME_None, TargetMontage, 1.0f, NAME_None,
      false, // bStopWhenAbilityEnds
      1.0f   // AnimRootMotionTranslationScale
  );

  if (PlayMontageTask) {
    PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_ToggleWeapon::OnMontageFinished);
    PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_ToggleWeapon::OnMontageInterrupted);
    PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_ToggleWeapon::OnMontageInterrupted);

    ActiveMontage = TargetMontage;
    PlayMontageTask->ReadyForActivation();

    // 제자리 루트 모션 몽타주(DefaultSlot) vs 이동 상체 몽타주(UpperBody 슬롯) 상태에 따른 풀바디 플래그 및 태그 관리
    const bool bIsTargetRoot = (TargetMontage == DrawMontage_Root || TargetMontage == SheatheMontage_Root);
    const FGameplayTag RootLockTag = NonTags::State_ToggleWeapon_Root.GetTag();
    if (bIsTargetRoot) {
      if (!bHasRequestedFullBody && CachedNonChar.IsValid()) {
        CachedNonChar->SetForceFullBody(true);
        bHasRequestedFullBody = true;
      }
      if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) {
        if (RootLockTag.IsValid()) ASC->SetLooseGameplayTagCount(RootLockTag, 1);
      }
    } else {
      if (bHasRequestedFullBody && CachedNonChar.IsValid()) {
        CachedNonChar->SetForceFullBody(false);
        bHasRequestedFullBody = false;
      }
      if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) {
        if (RootLockTag.IsValid()) ASC->SetLooseGameplayTagCount(RootLockTag, 0);
      }
    }

    // 몽타주가 활성화된 뒤 재생 시작 위치를 기존 진행 상황에 맞게 강제 설정
    AnimInstance->Montage_SetPosition(TargetMontage, NewPosition);
  } else {
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
  }
}

void UGA_ToggleWeapon::ClearMovementTimer() {
  if (UWorld* World = GetWorld()) {
    World->GetTimerManager().ClearTimer(MovementCheckTimerHandle);
  }
}
