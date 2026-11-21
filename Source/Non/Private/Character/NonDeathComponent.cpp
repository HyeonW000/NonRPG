#include "Character/NonDeathComponent.h"

#include "Character/NonCharacterBase.h"
#include "Ability/NonAbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/CombatSlotComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/NonPlayerController.h"
#include "Core/NonUIManagerComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

UNonDeathComponent::UNonDeathComponent() {
  PrimaryComponentTick.bCanEverTick = false;
  SetIsReplicatedByDefault(true);
}

ANonCharacterBase *UNonDeathComponent::GetOwnerCharacter() const {
  return Cast<ANonCharacterBase>(GetOwner());
}

void UNonDeathComponent::HandleDeath() {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner || Owner->bDied)
    return;

  Owner->bDied = true;
  Owner->SetForceFullBody(true); // 사망 시 즉시 풀바디 모드

  // AI 포위 슬롯 전체 해제
  if (UCombatSlotComponent *SlotComp = Owner->GetCombatSlotComponent()) {
    SlotComp->ReleaseAllSlots();
  }

  // 모든 디버프(출혈 등) 강제 제거
  if (UAbilitySystemComponent *ASC = Owner->GetAbilitySystemComponent()) {
    FGameplayTagContainer DebuffTags;
    DebuffTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Debuff"), false));
    ASC->RemoveActiveEffectsWithGrantedTags(DebuffTags);
  }

  // 이동/입력 차단
  if (UCharacterMovementComponent *Move = Owner->GetCharacterMovement()) {
    Move->StopMovementImmediately();
    Move->Velocity = FVector::ZeroVector;
  }
  if (AController *C = Owner->GetController()) {
    C->SetIgnoreMoveInput(true);
    C->SetIgnoreLookInput(true);
  }

  // 캡슐: 바닥은 유지하고 폰/카메라만 통과 (팝업/점프 버그 방지)
  if (UCapsuleComponent *Capsule = Owner->GetCapsuleComponent()) {
    Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
  }

  // 기존 액션 몽타주 중단 및 사망 몽타주 즉시 재생 (자동 블렌드 아웃 없음)
  Owner->StopAnimMontage();
  if (UAnimMontage *DeathMon = GetDeathMontage()) {
    DeathMon->bEnableAutoBlendOut = false;
    Owner->PlayAnimMontage(DeathMon);
    if (USkeletalMeshComponent *Mesh = Owner->GetMesh()) {
      if (UAnimInstance *AnimInst = Mesh->GetAnimInstance()) {
        if (FAnimMontageInstance *Inst = AnimInst->GetActiveInstanceForMontage(DeathMon)) {
          Inst->bEnableAutoBlendOut = false;
        }
      }
    }
  }

  // 3초 뒤 게임 오버 UI
  if (APlayerController *PC = Cast<APlayerController>(Owner->GetController())) {
    TWeakObjectPtr<APlayerController> WeakPC = PC;
    FTimerHandle Timer;
    GetWorld()->GetTimerManager().SetTimer(Timer, [WeakPC]() {
      if (ANonPlayerController *NonPC = Cast<ANonPlayerController>(WeakPC.Get())) {
        NonPC->ShowGameOverUI();
      }
    }, 3.0f, false);
  }
}

void UNonDeathComponent::FreezeDeathPose() {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (Owner && Owner->HasAuthority()) {
    Multicast_FreezeDeathPose();
  } else {
    Multicast_FreezeDeathPose_Implementation();
  }
}

void UNonDeathComponent::Multicast_FreezeDeathPose_Implementation() {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  USkeletalMeshComponent *Skel = Owner ? Owner->GetMesh() : nullptr;
  if (!Skel)
    return;

  // 마지막 포즈에서 애니/틱 정지 (시체가 다시 일어나지 않게 고정)
  Skel->bPauseAnims = true;
  Skel->SetComponentTickEnabled(false);
  Skel->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
  Skel->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
  Skel->CanCharacterStepUpOn = ECB_No;
}

void UNonDeathComponent::ApplyResurrectionPenalty() {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner)
    return;

  UAbilitySystemComponent *ASC = Owner->GetAbilitySystemComponent();
  const UNonAttributeSet *Attr = Owner->GetAttributeSet();

  if (ASC && Attr) {
    const float CurrentExp = Attr->GetExp();
    const float MaxExp = Attr->GetExpToNextLevel();
    const float NewExp = FMath::Max(0.f, CurrentExp - MaxExp * ResurrectionExpPenaltyRatio);

    ASC->SetNumericAttributeBase(UNonAttributeSet::GetExpAttribute(), NewExp);
    if (Owner->UIManagerComponent) {
      Owner->UIManagerComponent->UpdateEXP(NewExp, MaxExp);
    }
  }

  if (ASC && ResurrectionSicknessEffectClass) {
    FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
    EffectContext.AddInstigator(Owner, Owner);
    FGameplayEffectSpecHandle SpecHandle =
        ASC->MakeOutgoingSpec(ResurrectionSicknessEffectClass, 1.f, EffectContext);
    if (SpecHandle.IsValid()) {
      ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    }
  }
}

void UNonDeathComponent::Revive(bool bInPlace) {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner || !Owner->bDied)
    return;

  UAbilitySystemComponent *ASC = Owner->GetAbilitySystemComponent();
  const UNonAttributeSet *Attr = Owner->GetAttributeSet();

  // HP/MP 풀회복 및 사망 관련 태그/어빌리티 초기화
  if (ASC && Attr) {
    // 죽으면서 실행된 GA_Death 등 모든 능력 취소 (토글 무기, 회전 불가 버그 방지)
    ASC->CancelAllAbilities();
    ASC->SetNumericAttributeBase(UNonAttributeSet::GetHPAttribute(), Attr->GetMaxHP());
    ASC->SetNumericAttributeBase(UNonAttributeSet::GetMPAttribute(), Attr->GetMaxMP());
    ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag("State.Dead", false));
  }

  // 서버 및 모든 클라이언트 화면 복구
  if (Owner->HasAuthority()) {
    Multicast_OnRevive(bInPlace);
  } else {
    Multicast_OnRevive_Implementation(bInPlace);
  }

  // 부활 이벤트 발송 (서버에서 GA_Revive 실행 -> 몽타주 동기화) + 부활 무적
  if (ASC) {
    FGameplayEventData Payload;
    Payload.EventTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Revive"), false);
    Payload.Target = Owner;
    Payload.Instigator = Owner;
    Payload.EventMagnitude = bInPlace ? 1.0f : 0.0f;
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, Payload.EventTag, Payload);

    ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Invincible"), false));
  }
}

void UNonDeathComponent::Multicast_OnRevive_Implementation(bool bInPlace) {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner)
    return;

  Owner->bDied = false;

  // 사망 시 켰던 FullBody 모드 해제
  Owner->SetForceFullBody(false);

  // 1. 메쉬 애니메이션 정지 해제 및 잔여 사망 몽타주 종료
  if (USkeletalMeshComponent *Skel = Owner->GetMesh()) {
    Skel->bPauseAnims = false;
    Skel->SetComponentTickEnabled(true);
    if (UAnimInstance *AnimInst = Skel->GetAnimInstance()) {
      AnimInst->Montage_Stop(0.0f);
    }
  }

  // 2. 콜리전 복구
  if (UCapsuleComponent *Capsule = Owner->GetCapsuleComponent()) {
    Capsule->SetCollisionProfileName(FName(TEXT("Pawn")));
  }

  // 3. 컨트롤러 입력 복원
  if (AController *C = Owner->GetController()) {
    C->SetIgnoreMoveInput(false);
    C->SetIgnoreLookInput(false);
  }

  // 4. 이동 모드 걷기로 복구
  if (UCharacterMovementComponent *Move = Owner->GetCharacterMovement()) {
    Move->SetMovementMode(MOVE_Walking);
  }

  // 5. 무기 스탠스에 맞춰 Armed / Unarmed 복구
  if (Owner->GetWeaponStance() != EWeaponStance::Unarmed) {
    Owner->SetArmed(true);
  } else {
    if (UEquipmentComponent *Eq = Owner->FindComponentByClass<UEquipmentComponent>()) {
      Eq->ReattachSlotToHome(EEquipmentSlot::WeaponMain);
      Eq->ReattachSlotToHome(EEquipmentSlot::WeaponSub);
    }
    Owner->SetArmed(false);
  }
}

UAnimMontage *UNonDeathComponent::GetDeathMontage() const {
  const ANonCharacterBase *Owner = GetOwnerCharacter();
  if (Owner) {
    if (const TObjectPtr<UAnimMontage> *Found = StanceDeathMontages.Find(Owner->GetWeaponStance())) {
      if (*Found)
        return *Found;
    }
  }
  return DefaultDeathMontage;
}

UAnimMontage *UNonDeathComponent::GetReviveMontage() const {
  const ANonCharacterBase *Owner = GetOwnerCharacter();
  if (Owner) {
    if (const TObjectPtr<UAnimMontage> *Found = StanceReviveMontages.Find(Owner->GetWeaponStance())) {
      if (*Found)
        return *Found;
    }
  }
  return nullptr;
}
