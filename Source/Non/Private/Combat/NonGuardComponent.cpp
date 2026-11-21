#include "Combat/NonGuardComponent.h"

#include "Character/NonCharacterBase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

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
  const FVector LastInput = Owner->GetLastMovementInputVector();
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
                                            FGameplayTag ReactionTag) {
  if (!bGuarding) {
    bLastGuardSuccess = false;
    return false;
  }

  // 전방 180도 범위일 때만 가드 성공 (등 뒤에서 맞으면 가드 실패)
  bLastGuardSuccess = bFrontalHit;
  if (!bFrontalHit) {
    return false;
  }

  PlayGuardHitMontage(ReactionTag);
  InOutDamage *= GuardDamageMultiplier;
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

void UNonGuardComponent::PlayMontageOnOwner(UAnimMontage *Montage) const {
  ANonCharacterBase *Owner = GetOwnerCharacter();
  if (!Owner || !Montage)
    return;

  Owner->SetForceFullBody(true);

  // 가드 포즈/루프에 덮이지 않도록 BlendIn 없이 즉시 재생
  USkeletalMeshComponent *MeshComp = Owner->GetMesh();
  UAnimInstance *AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
  if (AnimInst) {
    AnimInst->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength,
                           0.0f, true);
  } else {
    Owner->PlayAnimMontage(Montage, 1.0f);
  }
}
