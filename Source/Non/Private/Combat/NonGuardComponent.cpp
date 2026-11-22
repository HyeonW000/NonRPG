#include "Combat/NonGuardComponent.h"

#include "Character/NonCharacterBase.h"
#include "Ability/NonAbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace {
EGuardDir8 Quantize8(float AngleDeg) {
  const float A = FMath::Fmod(AngleDeg + 360.f, 360.f);
  const int32 Idx = FMath::FloorToInt((A + 22.5f) / 45.f) % 8;
  return static_cast<EGuardDir8>(Idx);
}
} // namespace

UNonGuardComponent::UNonGuardComponent() {
  PrimaryComponentTick.bCanEverTick = false;
  SetIsReplicatedByDefault(true);
}

void UNonGuardComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty> &OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);
  DOREPLIFETIME(UNonGuardComponent, bGuarding);
}

ANonCharacterBase *UNonGuardComponent::GetOwnerCharacter() const {
  return Cast<ANonCharacterBase>(GetOwner());
}

void UNonGuardComponent::StartGuard() {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner || bGuarding || !Owner->IsArmed())
    return;

  // 설계서 5장 17: 스태미나 0일 때 가드 불가
  if (const UNonAttributeSet *Attr = Owner->GetAttributeSet()) {
    if (Attr->GetSP() <= 0.f) {
      return;
    }
  }

  // 1. 클라이언트면 서버에 요청
  if (!Owner->HasAuthority()) {
    ServerSetGuarding(true);
  }

  // 2. 로컬(서버 or 클라) 상태 적용
  bGuarding = true;
  ApplyGuardEnterEffects();
}

void UNonGuardComponent::StopGuard(bool bForceRelease) {
  if (!bGuarding)
    return;

  // 유저가 가드 키를 뗀 것이 아니면(bForceRelease == false) 가드를 끄지 않음
  if (!bForceRelease)
    return;

  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (Owner && !Owner->HasAuthority()) {
    ServerSetGuarding(false);
  }

  bGuarding = false;
  ApplyGuardExitEffects();
}

void UNonGuardComponent::ServerSetGuarding_Implementation(bool bEnable) {
  if (bEnable) {
    StartGuard(); // 서버에서 bGuarding=true -> 리플리케이션
  } else {
    StopGuard(true);
  }
}

void UNonGuardComponent::OnRep_IsGuarding() {
  // 리모트 클라이언트에서 변경 감지 시 같은 효과 적용
  if (bGuarding) {
    ApplyGuardEnterEffects();
  } else {
    ApplyGuardExitEffects();
  }
}

void UNonGuardComponent::ApplyGuardEnterEffects() {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner)
    return;

  if (UAbilitySystemComponent *ASC = Owner->GetAbilitySystemComponent()) {
    static const FGameplayTag Tag_DefenseBlock =
        FGameplayTag::RequestGameplayTag(TEXT("State.Defense.Block"), false);
    if (Tag_DefenseBlock.IsValid() && !ASC->HasMatchingGameplayTag(Tag_DefenseBlock)) {
      ASC->AddLooseGameplayTag(Tag_DefenseBlock);
    }
  }

  if (USkeletalMeshComponent *SkelComp = Owner->GetMesh()) {
    if (UAnimInstance *Anim = SkelComp->GetAnimInstance()) {
      Anim->Montage_StopGroupByName(0.1f, FName("DefaultGroup"));
    }
  }

  if (UCharacterMovementComponent *Move = Owner->GetCharacterMovement()) {
    Move->MaxWalkSpeed = GuardWalkSpeed;
    Move->BrakingDecelerationWalking = GuardBrakingDeceleration;
  }
}

void UNonGuardComponent::ApplyGuardExitEffects() {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner)
    return;

  bInBackZone = false;
  ClearGuardImpactState();

  if (UAbilitySystemComponent *ASC = Owner->GetAbilitySystemComponent()) {
    static const FGameplayTag Tag_DefenseBlock =
        FGameplayTag::RequestGameplayTag(TEXT("State.Defense.Block"), false);
    if (Tag_DefenseBlock.IsValid()) {
      ASC->RemoveLooseGameplayTag(Tag_DefenseBlock);
    }
  }

  // 속도 복원
  if (UCharacterMovementComponent *Move = Owner->GetCharacterMovement()) {
    if (Owner->bIsBackpedaling)
      Move->MaxWalkSpeed = Owner->WalkSpeed_Backpedal;
    else
      Move->MaxWalkSpeed =
          (Owner->WalkSpeed_Default > 0.f) ? Owner->WalkSpeed_Default : 600.f;
    Move->BrakingDecelerationWalking = 2048.f;
  }
}

void UNonGuardComponent::UpdateGuardDirAndSpeed() {
  if (!bGuarding)
    return;

  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner)
    return;

  UCharacterMovementComponent *Move = Owner->GetCharacterMovement();
  if (!Move)
    return;

  const float BaseYaw = Owner->GetControlRotation().Yaw;
  // 입력 벡터는 로컬 클라이언트에만 있으므로, 없으면 가속도 방향 사용 (서버에서도 같은 방향/속도 판정)
  FVector LastInput = Owner->GetLastMovementInputVector();
  if (LastInput.SizeSquared() <= KINDA_SMALL_NUMBER) {
    LastInput = Move->GetCurrentAcceleration().GetSafeNormal2D();
  }
  const FVector2D In(LastInput.X, LastInput.Y);
  const bool bHasInput = In.SizeSquared() > KINDA_SMALL_NUMBER;

  if (!bHasInput) {
    GuardDirAngle = 0.f;
    GuardDir8 = EGuardDir8::F;
  } else {
    const float InYaw = FMath::RadiansToDegrees(FMath::Atan2(In.Y, In.X));
    const float Rel = FMath::FindDeltaAngleDegrees(InYaw, BaseYaw);
    GuardDirAngle = Rel;
    GuardDir8 = Quantize8(Rel);
  }

  const float AbsRel = FMath::Abs(GuardDirAngle);
  if (!bInBackZone && AbsRel >= GuardBackEnterDeg)
    bInBackZone = true;
  else if (bInBackZone && AbsRel <= GuardBackExitDeg)
    bInBackZone = false;

  if (bInBackZone) {
    Move->MaxWalkSpeed = GuardBackSpeed;
    const float Spd = Move->Velocity.Size2D();
    if (Spd > GuardBackSpeed + 1.f) {
      const FVector NewXY =
          FVector(Move->Velocity.X, Move->Velocity.Y, 0.f).GetSafeNormal() *
          GuardBackSpeed;
      Move->Velocity = FVector(NewXY.X, NewXY.Y, Move->Velocity.Z);
    }
  } else {
    Move->MaxWalkSpeed = GuardWalkSpeed;
  }
}

bool UNonGuardComponent::ResolveIncomingHit(float &InOutDamage,
                                            bool bFrontalHit,
                                            FGameplayTag ReactionTag,
                                            AActor *Attacker) {
  if (!bGuarding) {
    bLastGuardSuccess = false;
    return false;
  }

  // 설계서 5장 15: 가드 불가 공격(Damage.Unblockable)은 가드 무시.
  // 공격 측이 공격 중 Damage.Unblockable 태그를 갖고 있으면(예: 공격 GA의 Activation Owned Tags) 가드 불가.
  // ReactionTag로 직접 넘긴 경우도 허용.
  static const FGameplayTag Tag_Unblockable =
      FGameplayTag::RequestGameplayTag(TEXT("Damage.Unblockable"), false);
  if (Tag_Unblockable.IsValid()) {
    bool bUnblockable = ReactionTag.IsValid() && ReactionTag.MatchesTag(Tag_Unblockable);
    if (!bUnblockable) {
      if (const IAbilitySystemInterface *ASI = Cast<IAbilitySystemInterface>(Attacker)) {
        if (const UAbilitySystemComponent *AttackerASC = ASI->GetAbilitySystemComponent()) {
          bUnblockable = AttackerASC->HasMatchingGameplayTag(Tag_Unblockable);
        }
      }
    }
    if (bUnblockable) {
      bLastGuardSuccess = false;
      return false;
    }
  }

  // 전방 180도 범위일 때만 가드 성공 (등 뒤에서 맞으면 가드 실패)
  bLastGuardSuccess = bFrontalHit;
  if (!bFrontalHit) {
    return false;
  }

  ANonCharacterBase *Owner = GetOwnerCharacter();
  const float OriginalDamage = InOutDamage;
  InOutDamage *= GuardDamageMultiplier;

  // 설계서 5장 15 & 17: 가드 피격 시 스태미나 소모 + 0 도달 시 가드 브레이크(State.Groggy 1.5초)
  if (Owner && Owner->HasAuthority()) {
    UAbilitySystemComponent *ASC = Owner->GetAbilitySystemComponent();
    const UNonAttributeSet *Attr = Owner->GetAttributeSet();
    if (ASC && Attr) {
      const float StaminaCost = OriginalDamage * GuardStaminaCostMultiplier;
      const float NewSP = FMath::Max(0.f, Attr->GetSP() - StaminaCost);
      ASC->SetNumericAttributeBase(UNonAttributeSet::GetSPAttribute(), NewSP);

      if (NewSP <= 0.f) {
        // 가드 브레이크: 가드 강제 해제 및 State.Groggy(1.5초) 부여
        StopGuard(true);

        static const FGameplayTag Tag_AbilityGuard =
            FGameplayTag::RequestGameplayTag(TEXT("Ability.Guard"), false);
        if (Tag_AbilityGuard.IsValid()) {
          FGameplayTagContainer CancelTags(Tag_AbilityGuard);
          ASC->CancelAbilities(&CancelTags);
        }

        // 서버에서 붙이는 태그라 클라이언트에도 복제(TagAndCountToAll)해야 클라 입력/어빌리티가 막힘.
        // 개수는 1로 고정(Set)해서, 브레이크가 연달아 와도 타이머 1회 해제로 깨끗이 풀리게 함.
        static const FGameplayTag Tag_Groggy =
            FGameplayTag::RequestGameplayTag(TEXT("State.Groggy"), false);
        static const FGameplayTag Tag_Stunned =
            FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"), false);
        constexpr EGameplayTagReplicationState RepState = EGameplayTagReplicationState::TagAndCountToAll;
        if (Tag_Groggy.IsValid()) {
          ASC->SetLooseGameplayTagCount(Tag_Groggy, 1, RepState);
        }
        if (Tag_Stunned.IsValid()) {
          ASC->SetLooseGameplayTagCount(Tag_Stunned, 1, RepState);
        }

        if (UWorld *W = GetWorld()) {
          TWeakObjectPtr<UAbilitySystemComponent> WeakASC = ASC;
          W->GetTimerManager().ClearTimer(GuardBreakTimerHandle);
          W->GetTimerManager().SetTimer(
              GuardBreakTimerHandle,
              [WeakASC]() {
                if (UAbilitySystemComponent *ValidASC = WeakASC.Get()) {
                  static const FGameplayTag Groggy =
                      FGameplayTag::RequestGameplayTag(TEXT("State.Groggy"), false);
                  static const FGameplayTag Stun =
                      FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"), false);
                  constexpr EGameplayTagReplicationState Rep = EGameplayTagReplicationState::TagAndCountToAll;
                  if (Groggy.IsValid()) ValidASC->SetLooseGameplayTagCount(Groggy, 0, Rep);
                  if (Stun.IsValid()) ValidASC->SetLooseGameplayTagCount(Stun, 0, Rep);
                }
              },
              GuardBreakDuration, false);
        }
        return false; // 가드 브레이크되었으므로 피격 리액션(GA_HitReaction) 발동 허용
      }
    }
  }

  PlayGuardHitMontage(ReactionTag);
  return true;
}

void UNonGuardComponent::PlayGuardHitMontage(FGameplayTag ImpactTag) {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner)
    return;

  // 가드 피격 시 가드 상태(속도 등) 보장
  if (!bGuarding) {
    StartGuard();
  }

  UAnimMontage *MontageToPlay = nullptr;

  // 1. 무기 스탠스별 (태그 -> 가드 몽타주) 맵 탐색
  if (const FStanceGuardHitMontageMap *FoundMap =
          StanceGuardHitMap.Find(Owner->GetWeaponStance())) {
    if (ImpactTag.IsValid()) {
      // 1-1. 태그 정확히 일치
      if (const TObjectPtr<UAnimMontage> *Found =
              FoundMap->TagMontageMap.Find(ImpactTag)) {
        if (*Found) {
          MontageToPlay = Found->Get();
        }
      }

      // 1-2. 부모/자식 태그 매칭
      if (!MontageToPlay) {
        for (const auto &Pair : FoundMap->TagMontageMap) {
          if (Pair.Key.IsValid() && Pair.Value &&
              (ImpactTag.MatchesTag(Pair.Key) || Pair.Key.MatchesTag(ImpactTag))) {
            MontageToPlay = Pair.Value.Get();
            break;
          }
        }
      }
    }

    // 1-3. 매칭 실패 시 맵의 첫 유효 몽타주
    if (!MontageToPlay) {
      for (const auto &Pair : FoundMap->TagMontageMap) {
        if (Pair.Value) {
          MontageToPlay = Pair.Value.Get();
          break;
        }
      }
    }
  }

  if (!MontageToPlay) {
    MontageToPlay = GuardHitMontage;
  }

  if (!MontageToPlay)
    return;

  PlayMontageOnOwner(MontageToPlay);

  if (Owner->HasAuthority()) {
    Multicast_PlayGuardHitMontage(MontageToPlay);
  }
}

void UNonGuardComponent::Multicast_PlayGuardHitMontage_Implementation(
    UAnimMontage *TargetMontage) {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (TargetMontage && Owner && !Owner->IsLocallyControlled()) {
    PlayMontageOnOwner(TargetMontage);
  }
}

void UNonGuardComponent::PlayMontageOnOwner(UAnimMontage *Montage) {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner || !Montage)
    return;

  if (!bIsGuardImpactActive) {
    bIsGuardImpactActive = true;
    Owner->SetForceFullBody(true);

    if (UAbilitySystemComponent *ASC = Owner->GetAbilitySystemComponent()) {
      static const FGameplayTag Tag_BlockHit =
          FGameplayTag::RequestGameplayTag(TEXT("Combat.BlockHit"), false);
      if (Tag_BlockHit.IsValid() && !ASC->HasMatchingGameplayTag(Tag_BlockHit)) {
        ASC->AddLooseGameplayTag(Tag_BlockHit);
      }
    }
  }

  // 가드 포즈/루프에 덮이지 않도록 BlendIn 없이 즉시 재생
  float PlayDuration = 0.f;
  USkeletalMeshComponent *MeshComp = Owner->GetMesh();
  UAnimInstance *AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
  if (AnimInst) {
    PlayDuration = AnimInst->Montage_Play(
        Montage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);
  } else {
    PlayDuration = Owner->PlayAnimMontage(Montage, 1.0f);
  }

  const float ImpactLockTime = FMath::Max(0.2f, PlayDuration > 0.f ? PlayDuration * 0.85f : 0.3f);
  if (UWorld *W = GetWorld()) {
    W->GetTimerManager().ClearTimer(GuardImpactTimerHandle);
    W->GetTimerManager().SetTimer(
        GuardImpactTimerHandle, this,
        &UNonGuardComponent::ClearGuardImpactState, ImpactLockTime, false);
  }
}

void UNonGuardComponent::ClearGuardImpactState() {
  if (UWorld *W = GetWorld()) {
    W->GetTimerManager().ClearTimer(GuardImpactTimerHandle);
  }

  if (!bIsGuardImpactActive)
    return;

  bIsGuardImpactActive = false;

  if (ANonCharacterBase *Owner = GetOwnerCharacter()) {
    // 가드 피격 직후 사망했으면 사망 몽타주용 풀바디를 끄지 않음
    if (!Owner->IsDead()) {
      Owner->SetForceFullBody(false);
    }

    if (UAbilitySystemComponent *ASC = Owner->GetAbilitySystemComponent()) {
      static const FGameplayTag Tag_BlockHit =
          FGameplayTag::RequestGameplayTag(TEXT("Combat.BlockHit"), false);
      if (Tag_BlockHit.IsValid()) {
        ASC->RemoveLooseGameplayTag(Tag_BlockHit);
      }
    }
  }
}
