#include "Character/NonDeathComponent.h"
#include "Core/NonGameplayTags.h"

#include "Character/NonCharacterBase.h"
#include "Ability/NonAbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/GameStateBase.h"
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

  // 늦게 접속한 사람용 사망 시각 (OnRep_DeathServerTime)
  if (Owner->HasAuthority() && GetWorld()) {
    if (const AGameStateBase *GS = GetWorld()->GetGameState()) {
      Owner->DeathServerTime = GS->GetServerWorldTimeSeconds();
    }
  }

  // AI 포위 슬롯 전체 해제
  if (UCombatSlotComponent *SlotComp = Owner->GetCombatSlotComponent()) {
    SlotComp->ReleaseAllSlots();
  }

  // 모든 디버프(출혈 등) 강제 제거
  if (UAbilitySystemComponent *ASC = Owner->GetAbilitySystemComponent()) {
    FGameplayTagContainer DebuffTags;
    DebuffTags.AddTag(NonTags::State_Debuff.GetTag());
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

  // 3초 뒤 게임 오버 UI (서버에서 부활 부적 보유 여부를 확인해 Client RPC로 전달)
  if (Owner->HasAuthority()) {
    if (ANonPlayerController *NonPC = Cast<ANonPlayerController>(Owner->GetController())) {
      TWeakObjectPtr<ANonPlayerController> WeakPC = NonPC;
      FTimerHandle Timer;
      GetWorld()->GetTimerManager().SetTimer(Timer, [WeakPC]() {
        if (ANonPlayerController *PC = WeakPC.Get()) {
          PC->ShowGameOverUI(PC->FindResurrectionAmuletSlot() != INDEX_NONE);
        }
      }, 3.0f, false);
    }
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

  // 사망 몽타주를 마지막 프레임으로 맞춤 (늦게 받았거나 못 받은 화면에서도 쓰러진 포즈)
  if (UAnimMontage *DeathMon = GetDeathMontage()) {
    if (UAnimInstance *AnimInst = Skel->GetAnimInstance()) {
      if (!AnimInst->Montage_IsPlaying(DeathMon)) {
        DeathMon->bEnableAutoBlendOut = false;
        AnimInst->Montage_PlayWithBlendIn(DeathMon, FAlphaBlendArgs(0.f), 1.0f);
      }
      AnimInst->Montage_SetPosition(DeathMon, FMath::Max(0.f, DeathMon->GetPlayLength() - 0.02f));
      AnimInst->Montage_SetPlayRate(DeathMon, 0.f);
    }
  }

  // 포즈가 반영될 시간을 준 뒤(0.15초) 애니/틱 정지 (부활했으면 건너뜀)
  {
    TWeakObjectPtr<ANonCharacterBase> WeakOwner(Owner);
    FTimerHandle PauseTimer;
    GetWorld()->GetTimerManager().SetTimer(PauseTimer, [WeakOwner]() {
      ANonCharacterBase *O = WeakOwner.Get();
      if (!O || !O->IsDead())
        return;
      if (USkeletalMeshComponent *S = O->GetMesh()) {
        S->bPauseAnims = true;
        S->SetComponentTickEnabled(false);
      }
    }, 0.15f, false);
  }

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

  if (Owner->HasAuthority()) {
    Owner->DeathServerTime = -1.f;
  }

  // HP/MP 풀회복 및 사망 관련 태그/어빌리티 초기화
  if (ASC && Attr) {
    // 죽으면서 실행된 GA_Death 등 모든 능력 취소 (토글 무기, 회전 불가 버그 방지)
    ASC->CancelAllAbilities();
    ASC->SetNumericAttributeBase(UNonAttributeSet::GetHPAttribute(), Attr->GetMaxHP());
    ASC->SetNumericAttributeBase(UNonAttributeSet::GetMPAttribute(), Attr->GetMaxMP());
    ASC->RemoveLooseGameplayTag(NonTags::State_Dead.GetTag());
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
    Payload.EventTag = NonTags::Effect_Revive.GetTag();
    Payload.Target = Owner;
    Payload.Instigator = Owner;
    Payload.EventMagnitude = bInPlace ? 1.0f : 0.0f;
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, Payload.EventTag, Payload);

    ASC->AddLooseGameplayTag(NonTags::State_Invincible.GetTag());
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

  // 4-1. 부활 모션 중에는 이동 금지 (몽타주 길이만큼), 끝나면 복구
  if (UAnimMontage *ReviveMon = GetReviveMontage()) {
    const float LockTime = ReviveMon->GetPlayLength();
    if (LockTime > 0.05f) {
      if (UCharacterMovementComponent *Move = Owner->GetCharacterMovement()) {
        Move->StopMovementImmediately();
        Move->DisableMovement();
      }
      AController *LocalC = Owner->IsLocallyControlled() ? Owner->GetController() : nullptr;
      if (LocalC) {
        LocalC->SetIgnoreMoveInput(true);
      }
      TWeakObjectPtr<ANonCharacterBase> WeakOwner(Owner);
      TWeakObjectPtr<AController> WeakC(LocalC);
      FTimerHandle ReviveLockTimer;
      GetWorld()->GetTimerManager().SetTimer(ReviveLockTimer, [WeakOwner, WeakC]() {
        if (AController *C = WeakC.Get()) {
          C->SetIgnoreMoveInput(false);
        }
        if (ANonCharacterBase *O = WeakOwner.Get()) {
          if (!O->IsDead()) {
            if (UCharacterMovementComponent *M = O->GetCharacterMovement()) {
              M->SetMovementMode(MOVE_Walking);
            }
          }
        }
      }, LockTime, false);
    }
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
