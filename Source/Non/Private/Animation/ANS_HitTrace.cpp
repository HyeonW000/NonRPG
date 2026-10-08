#include "Animation/ANS_HitTrace.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Camera/CameraShakeBase.h"
#include "Character/EnemyCharacter.h"
#include "Character/NonCharacterBase.h"
#include "Combat/NonDamageHelpers.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Equipment/EquipmentComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

// 소켓 소유 컴포넌트 찾기
USceneComponent *
UANS_HitTrace::ResolveSocketOwner(USkeletalMeshComponent *MeshComp) const {
  if (!MeshComp)
    return nullptr;

  // 1) 현재 메시에 소켓 둘 다 있으면 그대로 사용
  if (MeshComp->DoesSocketExist(StartSocket) &&
      MeshComp->DoesSocketExist(EndSocket)) {
    return MeshComp;
  }

  if (!bAutoFindSocketOwnerOnOwner)
    return nullptr;

  AActor *Owner = MeshComp->GetOwner();
  if (!Owner)
    return nullptr;

  // [New] 장착된 무기 액터(Spawned Actor)가 있다면, 그 액터 내부의 컴포넌트들을
  // 먼저 뒤진다.
  if (ANonCharacterBase *NonChar = Cast<ANonCharacterBase>(Owner)) {
    if (UEquipmentComponent *EqComp =
            NonChar->FindComponentByClass<UEquipmentComponent>()) {
      // 메인 무기 우선
      if (AActor *MainWeapon =
              EqComp->GetEquippedActor(EEquipmentSlot::WeaponMain)) {
        TArray<USceneComponent *> WeaponComps;
        MainWeapon->GetComponents<USceneComponent>(WeaponComps);
        for (USceneComponent *C : WeaponComps) {
          if (C && C->DoesSocketExist(StartSocket) &&
              C->DoesSocketExist(EndSocket)) {
            return C;
          }
        }
      }
      // 서브 무기도 체크
      if (AActor *SubWeapon =
              EqComp->GetEquippedActor(EEquipmentSlot::WeaponSub)) {
        TArray<USceneComponent *> WeaponComps;
        SubWeapon->GetComponents<USceneComponent>(WeaponComps);
        for (USceneComponent *C : WeaponComps) {
          if (C && C->DoesSocketExist(StartSocket) &&
              C->DoesSocketExist(EndSocket)) {
            return C;
          }
        }
      }
    }
  }

  // Owner의 모든 SkeletalMeshComponent 탐색
  TArray<USkeletalMeshComponent *> SkelComps;
  Owner->GetComponents<USkeletalMeshComponent>(SkelComps);

  // (a) 태그 우선
  if (!PreferredComponentTag.IsNone()) {
    for (USkeletalMeshComponent *C : SkelComps) {
      if (C && C->ComponentHasTag(PreferredComponentTag) &&
          C->DoesSocketExist(StartSocket) && C->DoesSocketExist(EndSocket)) {
        return C;
      }
    }
  }

  // (b) 그 외 컴포넌트에서 소켓 둘 다 있는 것
  for (USkeletalMeshComponent *C : SkelComps) {
    if (!C)
      continue;
    if (C->DoesSocketExist(StartSocket) && C->DoesSocketExist(EndSocket)) {
      return C;
    }
  }

  return nullptr;
}

void UANS_HitTrace::NotifyBegin(
    USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation,
    float TotalDuration, const FAnimNotifyEventReference &EventReference) {
  if (!MeshComp) return;

  // 🛡️ 모듈러 캐릭터 파츠(HeadMesh, HairMesh 등) 중복 노티파이 방지: 서브 파츠만 제외
  if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(MeshComp->GetOwner())) {
    if (MeshComp == NonChar->HeadMesh || MeshComp == NonChar->HairMesh || MeshComp == NonChar->EyebrowsMesh) {
      return;
    }
    NonChar->ClearCurrentSwingHitActors();
  } else {
    HitActors.Reset();
  }

  SocketOwnerComp = ResolveSocketOwner(MeshComp);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
  if (!SocketOwnerComp.IsValid()) {
  }
#endif
}

void UANS_HitTrace::NotifyEnd(USkeletalMeshComponent *MeshComp,
                              UAnimSequenceBase *Animation,
                              const FAnimNotifyEventReference &EventReference) {
  if (!MeshComp) return;

  // 🛡️ 모듈러 캐릭터 파츠 중복 방지
  if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(MeshComp->GetOwner())) {
    if (MeshComp == NonChar->HeadMesh || MeshComp == NonChar->HairMesh || MeshComp == NonChar->EyebrowsMesh) {
      return;
    }
    NonChar->ClearCurrentSwingHitActors();
  } else {
    HitActors.Reset();
  }

  SocketOwnerComp.Reset();
}

void UANS_HitTrace::NotifyTick(
    USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation,
    float FrameDeltaTime, const FAnimNotifyEventReference &EventReference) {
  if (!MeshComp)
    return;

  AActor *Owner = MeshComp->GetOwner();
  UWorld *World = MeshComp->GetWorld();
  if (!Owner || !World)
    return;

  // 🛡️ 모듈러 캐릭터 파츠(HeadMesh, HairMesh 등) 중복 실행 방지
  if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Owner)) {
    if (MeshComp == NonChar->HeadMesh || MeshComp == NonChar->HairMesh || MeshComp == NonChar->EyebrowsMesh) {
      return;
    }
  }

  if (bServerOnly && !Owner->HasAuthority()) {
    return;
  }

  ANonCharacterBase* NonCharOwner = Cast<ANonCharacterBase>(Owner);

  USceneComponent *Comp =
      SocketOwnerComp.IsValid() ? SocketOwnerComp.Get() : MeshComp;
  if (!Comp)
    return;

  if (!(Comp->DoesSocketExist(StartSocket) &&
        Comp->DoesSocketExist(EndSocket))) {
    USceneComponent *ReResolved = ResolveSocketOwner(MeshComp);
    if (!ReResolved)
      return;
    Comp = ReResolved;
    SocketOwnerComp = ReResolved;
  }

  const FVector Start = Comp->GetSocketLocation(StartSocket);
  const FVector End = Comp->GetSocketLocation(EndSocket);

  // Trace
  TArray<FHitResult> Hits;
  const ETraceTypeQuery TraceType =
      UEngineTypes::ConvertToTraceType(TraceChannel);

  TArray<AActor *> Ignore;
  Ignore.Add(Owner);

  UKismetSystemLibrary::SphereTraceMulti(
      World, Start, End, Radius, TraceType, false, Ignore, DebugDrawType, Hits,
      true, DebugTraceColor, DebugHitColor, DebugDrawTime);

  APawn *InstigatorPawn = Cast<APawn>(Owner);
  bool bShookOnce = false;

  for (const FHitResult &H : Hits) {
    AActor *Other = H.GetActor();
    if (!Other || Other == Owner)
      continue;
    if (!IsValidTarget(Other))
      continue;
    if (bSingleHitPerActor) {
      if (NonCharOwner) {
        if (NonCharOwner->HasHitActorInCurrentSwing(Other)) {
          continue;
        }
      } else if (HitActors.Contains(Other)) {
        continue;
      }
    }

    // 크리 여부 플래그 (히트마다 초기화)
    bool bWasCritical = false;

    // 여기서 Owner가 플레이어면 전투 상태 진입
    if (ANonCharacterBase *Player = Cast<ANonCharacterBase>(InstigatorPawn)) {
      Player->EnterCombatState();
    }

    // ── 실제 데미지 수치 계산 ──
    // 여기서는 AnimNotify 의 Damage 를 "계수(PowerScale)"로 사용
    float FinalDamage = Damage;

    if (InstigatorPawn) {
      float FinalScale = Damage;
      if (ANonCharacterBase* AttackerChar = Cast<ANonCharacterBase>(InstigatorPawn)) {
        FinalScale = Damage * AttackerChar->GetLastSkillDamageScale();
      }

      // 1) 공격자 기준 원 데미지 계산 (공격력/마력 + Min/Max 포함)
      const float RawDamage = UNonDamageHelpers::ComputeDamageFromAttributes(
          InstigatorPawn,
          FinalScale, // 갱신된 최종 계수 (노티파이 Damage * 스킬 계수)
          DamageStatType,
          &bWasCritical); // Physical / Magical (에디터에서 설정)

      // 2) 피격자 방어력/마저 적용
      if (RawDamage > 0.f) {
        FinalDamage = UNonDamageHelpers::ApplyDefenseReduction(
            Other, // 지금 맞고 있는 대상
            RawDamage, DamageStatType);
      }
    }

    // ── 데미지 적용 ──
    if (bUseGASDamage) {
      if (AEnemyCharacter *Victim = Cast<AEnemyCharacter>(Other)) {
        FVector HitLoc = Victim->GetActorLocation();
        if (!H.ImpactPoint.IsNearlyZero()) {
          HitLoc = H.ImpactPoint;
        } else if (!H.Location.IsNearlyZero()) {
          HitLoc = H.Location;
        }

        Victim->ApplyDamageAt(FinalDamage, Owner, HitLoc, bWasCritical, HitReactionTag);
      } else {
        const FVector Dir = (End - Start).GetSafeNormal();
        UGameplayStatics::ApplyPointDamage(
            Other, FinalDamage, Dir, H,
            InstigatorPawn ? InstigatorPawn->GetController() : nullptr, Owner,
            DamageType);
      }
    } else {
      const FVector Dir = (End - Start).GetSafeNormal();
      UGameplayStatics::ApplyPointDamage(Other, FinalDamage, Dir, H,
                                         Owner->GetInstigatorController(),
                                         Owner, DamageType);
    }

    // [New] 매 타격 시마다 커스텀 스턴 등의 상태이상(GE) 강제 부여 (서버에서만 실행)
    if (AdditionalEffect && Other->HasAuthority()) {
      if (UAbilitySystemComponent *ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Other)) {
        FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false);
        if (!DeadTag.IsValid() || !ASC->HasMatchingGameplayTag(DeadTag)) {
          FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
          Ctx.AddInstigator(InstigatorPawn, InstigatorPawn ? InstigatorPawn->GetController() : nullptr);
        
        // 무기를 휘두르는 주체(단순 폰이 아니라 내 캐릭터)에게서 데이터를 훔쳐옴!
        float EffectLevel = 1.0f;
        float StunDuration = 0.0f;
        float StatusEffectDuration = 0.0f;
        float StatusEffectChance = 0.0f;
        float StatusEffectValue = 0.0f;
        if (ANonCharacterBase* AttackerChar = Cast<ANonCharacterBase>(InstigatorPawn)) {
            EffectLevel = static_cast<float>(AttackerChar->GetLastSkillLevel());
            StunDuration = AttackerChar->GetLastSkillStunDuration();
            StatusEffectDuration = AttackerChar->GetLastSkillStatusEffectDuration();
            StatusEffectChance = AttackerChar->GetLastSkillStatusEffectChance();
            StatusEffectValue = AttackerChar->GetLastSkillStatusEffectValue();
        }

        // Spec을 만들어서 동적으로 시간(Duration)을 주입한 뒤 타겟에게 쏨!
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(AdditionalEffect, EffectLevel, Ctx);
        
        UE_LOG(LogTemp, Warning, TEXT("[StunDebug] Attacker: %s | Target: %s | StunDuration: %f | AdditionalEffect: %s"),
            *InstigatorPawn->GetName(), *Other->GetName(), StunDuration, AdditionalEffect ? *AdditionalEffect->GetName() : TEXT("Null"));

        if (SpecHandle.IsValid())
        {
            // 확률 검사 (지정된 확률이 0.001f 이상일 때만 확률 계산 진행, 0이면 100% 발동)
            bool bShouldApply = true;
            if (StatusEffectChance > 0.001f)
            {
                if (FMath::FRand() > StatusEffectChance)
                {
                    bShouldApply = false;
                }
            }

            if (bShouldApply)
            {
                // 데이터 에셋에 스턴 시간이 적혀 있을 때만 강제로 주입
                if (StunDuration > 0.001f)
                {
                    SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Data.StunDuration")), StunDuration);
                    SpecHandle.Data->SetSetByCallerMagnitude(FName(TEXT("Data.StunDuration")), StunDuration);
                    SpecHandle.Data->Duration = StunDuration; // C++ 강제 지속시간 오버라이드
                }
                // 데이터 에셋에 상태이상 지속시간이 적혀 있을 때만 강제로 주입
                if (StatusEffectDuration > 0.001f)
                {
                    SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Data.Duration")), StatusEffectDuration);
                    SpecHandle.Data->SetSetByCallerMagnitude(FName(TEXT("Data.Duration")), StatusEffectDuration);
                    SpecHandle.Data->Duration = StatusEffectDuration; // C++ 강제 지속시간 오버라이드
                }
                // 데이터 에셋에 상태이상 계수가 적혀 있을 때만 강제로 주입
                if (StatusEffectValue > 0.001f)
                {
                    SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Data.DamageScale")), StatusEffectValue);
                    SpecHandle.Data->SetSetByCallerMagnitude(FName(TEXT("Data.DamageScale")), StatusEffectValue);
                }
                FActiveGameplayEffectHandle ActiveHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
                UE_LOG(LogTemp, Warning, TEXT("[StunDebug] Apply Result - IsValid: %s | Duration: %f"),
                    ActiveHandle.IsValid() ? TEXT("True") : TEXT("False"), SpecHandle.Data->GetDuration());
            }
        }
      }
    }
  }

    // 카메라 셰이크(선택)
    if (CameraShakeClass && InstigatorPawn && !bShookOnce) {
      if (APlayerController *PC =
              Cast<APlayerController>(InstigatorPawn->GetController())) {
        PC->ClientStartCameraShake(CameraShakeClass, CameraShakeScale);
        bShookOnce = true;
      }
    }

    if (NonCharOwner) {
      NonCharOwner->AddHitActorInCurrentSwing(Other);
    } else {
      HitActors.Add(Other);
    }
  }
}

bool UANS_HitTrace::IsValidTarget(AActor *Other) const {
  if (!Other)
    return false;

  switch (Team) {
  case EHitTeamSide::Enemy:
    // 적이 쏜 HitTrace → 플레이어만 맞음
    return Cast<ANonCharacterBase>(Other) != nullptr;

  case EHitTeamSide::Player:
    // 플레이어가 쏜 HitTrace → 적만 맞음
    return Cast<AEnemyCharacter>(Other) != nullptr;

  default:
    return true;
  }
}
