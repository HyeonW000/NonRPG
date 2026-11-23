#include "Combat/NonGuardComponent.h"

#include "Character/NonCharacterBase.h"
#include "Equipment/EquipmentComponent.h"
#include "Ability/NonAbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
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

  // 3. 패링 창은 서버에서만 연다 (피격 판정이 서버에서 이뤄짐)
  if (Owner->HasAuthority()) {
    TryOpenParryWindow();
  }
}

bool UNonGuardComponent::HasShieldEquipped() const {
  const ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner)
    return false;
  // 현재 보조 무기 슬롯(WeaponSub)은 방패 전용. 양손 무기/지팡이를 들면 이 슬롯은 자동 해제됨.
  const UEquipmentComponent *Eq = Owner->FindComponentByClass<UEquipmentComponent>();
  return Eq && Eq->GetEquippedItemBySlot(EEquipmentSlot::WeaponSub) != nullptr;
}

void UNonGuardComponent::TryOpenParryWindow() {
  ParryWindowStartTime = -1.f;

  const UWorld *W = GetWorld();
  if (!W || ParryWindow <= 0.f || !HasShieldEquipped())
    return;

  const float Now = W->GetTimeSeconds();
  if (Now < ParryLockoutUntil)
    return; // 연타 방지: 직전 패링 실패 후 잠금 중 (일반 가드만)

  ParryWindowStartTime = Now;
  // 이번 창에서 패링이 안 나면 창 종료 + 잠금 시간까지 새 창을 열지 않음 (성공 시 즉시 해제)
  ParryLockoutUntil = Now + ParryWindow + ParryFailLockout;
}

bool UNonGuardComponent::IsInParryWindow() const {
  const UWorld *W = GetWorld();
  if (!W || ParryWindowStartTime < 0.f)
    return false;
  return (W->GetTimeSeconds() - ParryWindowStartTime) <= ParryWindow;
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
  ParryWindowStartTime = -1.f; // 가드를 풀면 패링 창도 닫힘 (잠금 시간은 유지)
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
                                            AActor *Attacker,
                                            bool *bOutParried) {
  if (bOutParried) {
    *bOutParried = false;
  }

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

  // 설계서 5장 16: 패링 — 방패 착용 + 정면 + 가드 입력 후 ParryWindow 안 → 피해 0, SP 소모 0
  // (가드 불가 공격은 위에서 이미 걸러짐)
  if (Owner && Owner->HasAuthority() && IsInParryWindow()) {
    InOutDamage = 0.f;
    if (bOutParried) {
      *bOutParried = true;
    }
    // 성공하면 잠금 해제 → 연속 공격도 다시 가드를 눌러 패링 가능. 같은 창 안의 다단 히트도 패링 유지.
    if (const UWorld *W = GetWorld()) {
      ParryLockoutUntil = W->GetTimeSeconds();
    }
    PlayGuardHitMontage(ReactionTag, true);
    return true;
  }

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

  ApplyGuardPush(Attacker, ReactionTag);
  PlayGuardHitMontage(ReactionTag);
  return true;
}

bool UNonGuardComponent::IsGuardPushActive() const {
  const UWorld *W = GetWorld();
  return W && GuardPushLegLockUntil > 0.f && W->GetTimeSeconds() < GuardPushLegLockUntil;
}

void UNonGuardComponent::ApplyGuardPush(AActor *Attacker, FGameplayTag ReactionTag) {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!bEnableGuardPush || !Owner || !Owner->HasAuthority())
    return;

  FVector Dir = Attacker ? (Owner->GetActorLocation() - Attacker->GetActorLocation())
                         : -Owner->GetActorForwardVector();
  Dir.Z = 0.f;
  if (Dir.IsNearlyZero()) {
    Dir = -Owner->GetActorForwardVector();
    Dir.Z = 0.f;
  }
  Dir = Dir.GetSafeNormal();

  const FString TagStr = ReactionTag.ToString();
  const bool bHeavy = TagStr.Contains(TEXT("Heavy"), ESearchCase::IgnoreCase) ||
                      TagStr.Contains(TEXT("Medium"), ESearchCase::IgnoreCase) ||
                      TagStr.Contains(TEXT("Knockdown"), ESearchCase::IgnoreCase) ||
                      TagStr.Contains(TEXT("Knockback"), ESearchCase::IgnoreCase);
  const float Dist = bHeavy ? GuardPushDistanceHeavy : GuardPushDistanceLight;
  const float Duration = FMath::Max(0.02f, GuardPushDuration);
  if (Dist <= 0.f || Dir.IsNearlyZero())
    return;
  const float Speed = Dist / Duration;

  ApplyGuardPushLocal(Dir, Speed, Duration);
  // 원격 클라이언트가 조종하는 캐릭터는 본인 클라이언트에도 같은 힘을 걸어야 위치 보정(끊김)이 없음
  if (!Owner->IsLocallyControlled() && Owner->IsPlayerControlled()) {
    Client_ApplyGuardPush(Dir, Speed, Duration);
  }
  UE_LOG(LogTemp, Verbose, TEXT("[GuardPush] %s pushed %.0fcm (%s) by %s"), *Owner->GetName(), Dist,
         *TagStr, *GetNameSafe(Attacker));
}

void UNonGuardComponent::Client_ApplyGuardPush_Implementation(FVector Dir, float Speed, float Duration) {
  ApplyGuardPushLocal(Dir, Speed, Duration);
}

void UNonGuardComponent::ApplyGuardPushLocal(const FVector &Dir, float Speed, float Duration) {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  UCharacterMovementComponent *Move = Owner ? Owner->GetCharacterMovement() : nullptr;
  if (!Move)
    return;

  static const FName GuardPushName(TEXT("GuardPush"));
  Move->RemoveRootMotionSource(GuardPushName); // 연속 가드 시 겹치지 않게 이전 밀림 제거

  TSharedPtr<FRootMotionSource_ConstantForce> Src = MakeShared<FRootMotionSource_ConstantForce>();
  Src->InstanceName = GuardPushName;
  Src->AccumulateMode = ERootMotionAccumulateMode::Override;
  Src->Priority = 5;
  Src->Force = Dir * Speed;
  Src->Duration = Duration;
  Src->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
  Src->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
  Move->ApplyRootMotionSource(Src);
}

void UNonGuardComponent::PlayGuardHitMontage(FGameplayTag ImpactTag, bool bParry) {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner)
    return;

  // 가드 피격 시 가드 상태(속도 등) 보장
  if (!bGuarding) {
    StartGuard();
  }

  // 패링 성공 시 전용 몽타주 우선 (없으면 아래 가드 피격 몽타주로 대체)
  UAnimMontage *MontageToPlay = (bParry && ParryMontage) ? ParryMontage.Get() : nullptr;

  // 1. 무기 스탠스별 (태그 -> 가드 몽타주) 맵 탐색
  const FStanceGuardHitMontageMap *FoundMap =
      MontageToPlay ? nullptr : StanceGuardHitMap.Find(Owner->GetWeaponStance());
  if (FoundMap) {
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
  // 서버는 PlayGuardHitMontage에서 이미 재생함. 본인 클라이언트 포함 나머지 모두 재생
  // (가드 판정은 서버 전용이라 본인 클라이언트가 직접 재생하는 경로가 없음)
  if (TargetMontage && Owner && !Owner->HasAuthority()) {
    PlayMontageOnOwner(TargetMontage);
  }
}

void UNonGuardComponent::PlayMontageOnOwner(UAnimMontage *Montage) {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner || !Montage)
    return;

  // 가드 밀림 동안 다리 고정 (서버, 본인, 다른 클라이언트 모두 이 함수로 몽타주를 재생)
  if (bEnableGuardPush) {
    if (const UWorld *W = GetWorld()) {
      GuardPushLegLockUntil = W->GetTimeSeconds() + FMath::Max(0.02f, GuardPushDuration) + 0.1f;
    }
  }

  if (!bIsGuardImpactActive) {
    bIsGuardImpactActive = true;
    bImpactForcedFullBody = bGuardHitFullBody;
    if (bImpactForcedFullBody) {
      Owner->SetForceFullBody(true);
    }

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
    if (bImpactForcedFullBody && !Owner->IsDead()) {
      Owner->SetForceFullBody(false);
    }
    bImpactForcedFullBody = false;

    if (UAbilitySystemComponent *ASC = Owner->GetAbilitySystemComponent()) {
      static const FGameplayTag Tag_BlockHit =
          FGameplayTag::RequestGameplayTag(TEXT("Combat.BlockHit"), false);
      if (Tag_BlockHit.IsValid()) {
        ASC->RemoveLooseGameplayTag(Tag_BlockHit);
      }
    }
  }
}
