// GA_GroundTarget.cpp
// 모든 몽타주/AOE/캐스팅 파라미터를 SkillDataAsset의 FAOEConfig 에서 읽음
// → GA 블루프린트 1개로 모든 장판 스킬 처리

#include "Ability/Skills/GA_GroundTarget.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Character/NonCharacterBase.h"
#include "Combat/DamageAOE.h"
#include "Core/NonUIManagerComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"
#include "TimerManager.h"
#include "UI/InGameHUD.h"

// ── 헬퍼: UIManager → InGameHUD ──────────────────────────────────
static UInGameHUD *GetHUDFor(const FGameplayAbilityActorInfo *Info) {
  if (!Info || !Info->AvatarActor.IsValid())
    return nullptr;
  if (UNonUIManagerComponent *M = Cast<UNonUIManagerComponent>(
          Info->AvatarActor.Get()->GetComponentByClass(
              UNonUIManagerComponent::StaticClass())))
    return M->GetInGameHUD();
  return nullptr;
}

// ─────────────────────────────────────────────────────────────────
UGA_GroundTarget::UGA_GroundTarget() {
  InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

// ═════════════════════════════════════════════════════════════════
// ActivateAbility
// ═════════════════════════════════════════════════════════════════
void UGA_GroundTarget::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo *ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData *TriggerEventData) {
  if (!ActorInfo || !ActorInfo->OwnerActor.IsValid()) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    return;
  }

  AActor *Owner = ActorInfo->OwnerActor.Get();
  UAbilitySystemComponent *ASC = ActorInfo->AbilitySystemComponent.Get();
  if (!Owner || !ASC) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    return;
  }

  USkillManagerComponent *SkillMgr =
      Owner->FindComponentByClass<USkillManagerComponent>();
  if (!SkillMgr) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    return;
  }

  const FName SkillId = SkillMgr->ConsumePendingSkillId();
  if (SkillId.IsNone()) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    return;
  }

  USkillDataAsset *DA = SkillMgr->GetDataAsset();
  if (!DA) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    return;
  }

  CachedRow = DA->Skills.Find(SkillId);
  if (!CachedRow || !CachedRow->bIsGroundTarget) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    return;
  }

  // 레벨/계수
  const int32 Level = SkillMgr->GetSkillLevel(SkillId);
  CurrentSkillLevel = FMath::Max(1, Level);
  CurrentDamageScale = 1.f;
  float StunDuration = 0.f;

  if (CachedRow->LevelScalars.IsValidIndex(CurrentSkillLevel - 1))
    CurrentDamageScale = CachedRow->LevelScalars[CurrentSkillLevel - 1];
  if (CachedRow->StunDurations.IsValidIndex(CurrentSkillLevel - 1))
    StunDuration = CachedRow->StunDurations[CurrentSkillLevel - 1];

  if (ANonCharacterBase *NC =
          Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get())) {
    NC->SetLastSkillDamageScale(CurrentDamageScale);
    NC->SetLastSkillLevel(CurrentSkillLevel);
    NC->SetLastSkillStunDuration(StunDuration);
    NC->SetForceFullBody(true);
  }

  // SP 소모
  const float SPCost = SkillMgr->GetStaminaCost(*CachedRow, Level);
  if (SPCost > 0.f) {
    const FGameplayAttribute SPAttr = UNonAttributeSet::GetSPAttribute();
    if (ASC->GetNumericAttribute(SPAttr) < SPCost) {
      EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
      return;
    }
    if (Owner->HasAuthority())
      ASC->SetNumericAttributeBase(
          SPAttr, FMath::Max(0.f, ASC->GetNumericAttribute(SPAttr) - SPCost));
  }

  if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) {
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    return;
  }

  StartCastingPhase();
}

// ═════════════════════════════════════════════════════════════════
// [1단계] 캐스팅
// ═════════════════════════════════════════════════════════════════
void UGA_GroundTarget::StartCastingPhase() {
  CurrentPhase = EGroundTargetPhase::Casting;
  RegisterHitCancelListener();

  const float CastTime = CachedRow ? CachedRow->AOEConfig.CastTime : 2.f;

  UAnimMontage *CastMontage =
      CachedRow ? CachedRow->CastingMontage.Get() : nullptr;
  if (CastMontage) {
    UAbilityTask_PlayMontageAndWait *Task =
        UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("CastingMontageTask"), CastMontage, 1.f);
    if (Task)
      Task->ReadyForActivation();
  }

  // 이동 감지 Tick
  GetWorld()->GetTimerManager().SetTimer(
      CastingTickHandle, this, &UGA_GroundTarget::CastingTick, 0.05f, true);

  // HUD + 이벤트
  OnCastingStarted(CastTime);
  if (UInGameHUD *HUD = GetHUDFor(CurrentActorInfo))
    HUD->StartCasting(CastTime);

  if (CastTime <= 0.f)
    OnCastingTimerExpired();
  else
    GetWorld()->GetTimerManager().SetTimer(
        CastTimerHandle, this, &UGA_GroundTarget::OnCastingTimerExpired,
        CastTime, false);
}

void UGA_GroundTarget::OnCastingTimerExpired() {
  GetWorld()->GetTimerManager().ClearTimer(CastingTickHandle);

  OnCastingCompleted();
  if (UInGameHUD *HUD = GetHUDFor(CurrentActorInfo))
    HUD->StopCasting();

  StartTargetingPhase();
}

// ─────────────────────────────────────────────────────────────────
void UGA_GroundTarget::CastingTick() {
  if (!CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid())
    return;
  APawn *Pawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
  if (Pawn && Pawn->GetVelocity().SizeSquared2D() > 100.f * 100.f)
    OnCancelTarget();
}

// ─────────────────────────────────────────────────────────────────
// 피격 캔슬
// ─────────────────────────────────────────────────────────────────
void UGA_GroundTarget::RegisterHitCancelListener() {
  UAbilitySystemComponent *ASC =
      CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get()
                       : nullptr;
  if (!ASC)
    return;
  const FGameplayTag Tag =
      FGameplayTag::RequestGameplayTag(TEXT("State.HitReacting"));
  HitTagEventHandle =
      ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
          .AddUObject(this, &UGA_GroundTarget::OnHitTagChanged);
}

void UGA_GroundTarget::UnregisterHitCancelListener() {
  UAbilitySystemComponent *ASC =
      CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get()
                       : nullptr;
  if (!ASC)
    return;
  const FGameplayTag Tag =
      FGameplayTag::RequestGameplayTag(TEXT("State.HitReacting"));
  ASC->UnregisterGameplayTagEvent(HitTagEventHandle, Tag,
                                  EGameplayTagEventType::NewOrRemoved);
}

void UGA_GroundTarget::OnHitTagChanged(const FGameplayTag Tag, int32 NewCount) {
  if (NewCount > 0 && IsActive())
    OnCancelTarget();
}

// ═════════════════════════════════════════════════════════════════
// [2단계] 조준
// ═════════════════════════════════════════════════════════════════
void UGA_GroundTarget::StartTargetingPhase() {
  CurrentPhase = EGroundTargetPhase::Targeting;

  // 카메라 잠금 및 시작 시 마우스 커서를 캐릭터 정면으로 강제 위치
  if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid()) {
    if (ANonCharacterBase *NC =
            Cast<ANonCharacterBase>(CurrentActorInfo->AvatarActor.Get())) {
      NC->SetLookInputBlocked(true);

      // 평소에 마우스가 숨겨져 있어서 엉뚱한 곳에서 장판이 시작되는 것을 방지하기 위해,
      // 조준이 시작되는 순간 보이지 않는 마우스 커서를 캐릭터 정면(조금 앞)으로 강제 소환(Warp)합니다.
      if (APlayerController* PC = Cast<APlayerController>(NC->GetController()))
      {
          FVector ForwardStartLoc = NC->GetActorLocation() + (NC->GetActorForwardVector() * 400.f);
          FVector2D ScreenPos;
          if (PC->ProjectWorldLocationToScreen(ForwardStartLoc, ScreenPos))
          {
              PC->SetMouseLocation(FMath::RoundToInt(ScreenPos.X), FMath::RoundToInt(ScreenPos.Y));
          }
      }
    }
  }

  // 데칼 스폰 (AOEConfig.DecalClass 사용)
  TSubclassOf<AActor> DClass =
      CachedRow ? CachedRow->AOEConfig.DecalClass : nullptr;
  const float Range = CachedRow ? CachedRow->AOEConfig.MaxTargetRange : 1000.f;
  FVector InitLoc;
  GetMouseGroundLocation(InitLoc, Range);
  SpawnDecal(InitLoc, DClass);

  // 2단계: 조준 시작 (카메라 잠금, 데칼 스폰, 틱 타이머 등록)
  // 입력 처리는 TargetingTick에서 직접 체크 (Enhanced Input/GAS 매핑 불필요)
  GetWorld()->GetTimerManager().SetTimer(TargetingTickHandle, this,
                                         &UGA_GroundTarget::TargetingTick,
                                         0.016f, true);
}

void UGA_GroundTarget::TargetingTick() {
  const float Range = CachedRow ? CachedRow->AOEConfig.MaxTargetRange : 1000.f;

  FVector MouseLoc;
  if (GetMouseGroundLocation(MouseLoc, Range) && ActiveDecal.IsValid()) {
    ActiveDecal->SetActorLocation(MouseLoc);
  }

  // 이동 감지 및 입력 체크
  if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid()) {
    if (APawn *Pawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get())) {
      // 이동 캔슬 체크
      if (Pawn->GetVelocity().SizeSquared2D() > 100.f * 100.f) {
        OnCancelTarget();
        return;
      }

      // 마우스 클릭(확정/취소) 체크
      if (APlayerController *PC =
              Cast<APlayerController>(Pawn->GetController())) {
        // 타이머 틱 레이턴시로 인해 클릭이 간헐적으로 씹히는 현상을 완벽
        // 차단하기 위해 IsInputKeyDown 사용
        if (PC->IsInputKeyDown(EKeys::LeftMouseButton)) {
          OnConfirmTarget();
          return;
        }

        if (PC->IsInputKeyDown(EKeys::RightMouseButton)) {
          OnCancelTarget();
          return;
        }
      }
    }
  }
}

void UGA_GroundTarget::OnConfirmTarget() {
  GetWorld()->GetTimerManager().ClearTimer(TargetingTickHandle);

  const float Range = CachedRow ? CachedRow->AOEConfig.MaxTargetRange : 1000.f;
  GetMouseGroundLocation(ConfirmedTargetLocation, Range);
  DestroyDecal();
  UnregisterHitCancelListener();

  if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid()) {
    if (ANonCharacterBase *NC =
            Cast<ANonCharacterBase>(CurrentActorInfo->AvatarActor.Get())) {
      NC->SetLookInputBlocked(false);
    }
  }

  StartReleasingPhase();
}

void UGA_GroundTarget::OnCancelTarget() {
  GetWorld()->GetTimerManager().ClearTimer(CastTimerHandle);
  GetWorld()->GetTimerManager().ClearTimer(CastingTickHandle);
  GetWorld()->GetTimerManager().ClearTimer(TargetingTickHandle);
  UnregisterHitCancelListener();
  DestroyDecal();

  if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid()) {
    if (ANonCharacterBase *NC =
            Cast<ANonCharacterBase>(CurrentActorInfo->AvatarActor.Get())) {
      NC->SetLookInputBlocked(false);
    }
  }

  OnSkillCancelled();
  if (UInGameHUD *HUD = GetHUDFor(CurrentActorInfo))
    HUD->StopCasting();
  EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true,
             true);
}

// ═════════════════════════════════════════════════════════════════
// [3단계] 격발
// ═════════════════════════════════════════════════════════════════
void UGA_GroundTarget::StartReleasingPhase() {
  CurrentPhase = EGroundTargetPhase::Releasing;

  // 클릭 즉시 데미지/장판 스폰
  SpawnAOE();

  // 다른 슬롯(UpperBody 구별)을 사용할 경우 기존 Cast(DefaultSlot)가 살아있어서
  // 충돌하는 것을 방지
  if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid()) {
    if (ACharacter *C = Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get())) {
      UAnimMontage *CastMontage =
          CachedRow ? CachedRow->CastingMontage.Get() : nullptr;
      if (CastMontage)
        C->StopAnimMontage(CastMontage);

      // [핵심 픽스] 캐스팅 단계에서 다리 이동을 막기 위해 켜뒀던 ForceFullBody를 꺼줍니다!
      // 이걸 꺼야만 유저님의 애님 그래프가 Ground Speed > 1 일 때 UpperBodyPose 블렌딩으로 정상 전환됩니다.
      if (ANonCharacterBase *NC = Cast<ANonCharacterBase>(C)) {
        NC->SetForceFullBody(false);
      }
    }
  }

  UAnimMontage *RelMontage = CachedRow ? CachedRow->Montage.Get() : nullptr;
  if (RelMontage) {
    UAbilityTask_PlayMontageAndWait *Task =
        UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("ShootMontageTask"), RelMontage, 1.f);
    if (Task) {
      Task->OnCompleted.AddDynamic(this,
                                   &UGA_GroundTarget::OnReleaseMontageFinished);
      Task->OnCancelled.AddDynamic(
          this, &UGA_GroundTarget::OnReleaseMontageCancelled);
      Task->OnInterrupted.AddDynamic(
          this, &UGA_GroundTarget::OnReleaseMontageCancelled);
      Task->ReadyForActivation();
      return;
    }
  }

  // 몽타주가 없거나 실행 실패 시 즉시 종료
  if (IsActive())
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true,
               false);
}

void UGA_GroundTarget::OnReleaseMontageFinished() {
  // 몽타주가 끝났으므로 어빌리티 최종 종료
  if (IsActive())
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true,
               false);
}

void UGA_GroundTarget::OnReleaseMontageCancelled() {
  if (IsActive())
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true,
               true);
}

void UGA_GroundTarget::SpawnAOE() {
  if (!IsActive() || !CachedRow)
    return;

  const FAOEConfig &Cfg = CachedRow->AOEConfig;
  if (!Cfg.AOEClass || !GetWorld()) {
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
               false, false);
    return;
  }

  AActor *Owner =
      CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;

  const FVector SpawnLoc = ConfirmedTargetLocation + FVector(0.f, 0.f, 10.f);
  ADamageAOE *AOE = GetWorld()->SpawnActorDeferred<ADamageAOE>(
      Cfg.AOEClass, FTransform(FRotator::ZeroRotator, SpawnLoc), Owner,
      Cast<APawn>(Owner), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

  if (AOE) {
    // DataAsset 파라미터 전달
    AOE->bDebugDraw = Cfg.bDebugDraw;
    AOE->bServerOnly = Cfg.bServerOnly;

    if (Cfg.AdditionalEffects.Num() > 0) {
      AOE->AdditionalEffect = Cfg.AdditionalEffects[0];
    }

    // Shape별로 Configure 호출 (BeginPlay 전)
    switch (Cfg.Shape) {
    case EAOEConfigShape::Sphere:
      AOE->ConfigureSphere(Cfg.Radius, CurrentDamageScale, Cfg.Lifespan);
      break;
    case EAOEConfigShape::Box:
      AOE->ConfigureBox(Cfg.BoxExtent, CurrentDamageScale, Cfg.Lifespan);
      break;
    case EAOEConfigShape::Capsule:
      // Capsule 전용 Configure 가 없으면 Sphere 로 폴백
      AOE->ConfigureSphere(Cfg.Radius, CurrentDamageScale, Cfg.Lifespan);
      break;
    }

    AOE->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLoc));
    OnAOESpawned(AOE, ConfirmedTargetLocation);
  }

  // 여기서 EndAbility를 부르지 않음! (몽타주 끝날 때까지 스킬 유지)
}

// ═════════════════════════════════════════════════════════════════
// EndAbility
// ═════════════════════════════════════════════════════════════════
void UGA_GroundTarget::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo *ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled) {
  if (GetWorld()) {
    GetWorld()->GetTimerManager().ClearTimer(CastTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(CastingTickHandle);
    GetWorld()->GetTimerManager().ClearTimer(TargetingTickHandle);
  }
  UnregisterHitCancelListener();
  DestroyDecal();

  if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    if (ANonCharacterBase *NC =
            Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get())) {
      NC->SetLookInputBlocked(false);
      NC->SetForceFullBody(false);
    }

  Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility,
                    bWasCancelled);
}

// ─────────────────────────────────────────────────────────────────
// 유틸리티
// ─────────────────────────────────────────────────────────────────
bool UGA_GroundTarget::GetMouseGroundLocation(FVector &OutLocation,
                                              float MaxRange) const {
  if (!CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid())
    return false;
  APawn *Pawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
  if (!Pawn)
    return false;
  APlayerController *PC = Cast<APlayerController>(Pawn->GetController());
  if (!PC)
    return false;

  // 카메라 기준 마우스 레이
  FVector WorldLoc, WorldDir;
  if (!PC->DeprojectMousePositionToWorld(WorldLoc, WorldDir))
    return false;

  FCollisionQueryParams Params;
  Params.AddIgnoredActor(Pawn);
  const FVector PawnLoc = Pawn->GetActorLocation();

  // [마법의 수학 투사] 장애물(돌, 나무)에 걸리는 것을 무시하기 위해 카메라 뷰를
  // 캐릭터 발밑 절대 평면에 수식 투사!
  FVector PlaneLoc;
  if (FMath::Abs(WorldDir.Z) > 0.001f) {
    float T = (PawnLoc.Z - WorldLoc.Z) / WorldDir.Z;
    if (T > 0.f)
      PlaneLoc = WorldLoc + WorldDir * T;
    else {
      // T가 음수면 카메라가 하늘이나 벼랑을 향하고 있다는 뜻! (이전 Snap 문제
      // 원인) 카메라가 가리키는 X, Y 방향성을 그대로 가져와서 그 방향으로 투사
      FVector FlatDir = FVector(WorldDir.X, WorldDir.Y, 0.f).GetSafeNormal();
      if (FlatDir.IsNearlyZero())
        FlatDir = Pawn->GetActorForwardVector();
      PlaneLoc = PawnLoc + FlatDir * MaxRange;
    }
  } else {
    PlaneLoc = PawnLoc + Pawn->GetActorForwardVector() * MaxRange;
  }

  // 수평 거리 제어 (MaxRange 클램프)
  FVector Delta = PlaneLoc - PawnLoc;
  Delta.Z = 0.f; // 2D 평면 거리
  const float Dist = Delta.Size();

  if (Dist > MaxRange && Dist > 0.001f) {
    const float S = MaxRange / Dist;
    PlaneLoc.X = PawnLoc.X + Delta.X * S;
    PlaneLoc.Y = PawnLoc.Y + Delta.Y * S;
  }

  // 구해진 이상적인 위치(허공/바닥 아래일 수 있음)에서 진짜 지형 바닥을 찾기
  // 위해 수직 레이저 트레이스 발사
  FVector TraceStart = PlaneLoc;
  TraceStart.Z = PawnLoc.Z + 2000.f; // 머리 한참 위에서 시작
  FVector TraceEnd = TraceStart - FVector(0, 0, 4000.f);

  FHitResult DownHit;
  // 이제 순수하게 위에서 아래로만 쏘기 때문에, 시야를 가리는 나무 같은 것에
  // 전혀 안 걸립니다.
  if (GetWorld()->LineTraceSingleByChannel(DownHit, TraceStart, TraceEnd,
                                           ECC_Visibility, Params))
    PlaneLoc.Z = DownHit.ImpactPoint.Z;
  else
    PlaneLoc.Z = PawnLoc.Z; // 안전 장치

  OutLocation = PlaneLoc;
  return true;
}

void UGA_GroundTarget::SpawnDecal(const FVector &Location,
                                  TSubclassOf<AActor> DecalClass) {
  if (!DecalClass || !GetWorld())
    return;
  FActorSpawnParameters P;
  P.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  ActiveDecal = GetWorld()->SpawnActor<AActor>(DecalClass, Location,
                                               FRotator::ZeroRotator, P);
}

void UGA_GroundTarget::DestroyDecal() {
  if (ActiveDecal.IsValid()) {
    ActiveDecal->Destroy();
    ActiveDecal.Reset();
  }
}

// ─────────────────────────────────────────────────────────────────
// 입력 액션 콜백
// ─────────────────────────────────────────────────────────────────
void UGA_GroundTarget::OnConfirmTargetInput(float TimeWaited) {
  OnConfirmTarget();
}

void UGA_GroundTarget::OnCancelTargetInput(float TimeWaited) {
  OnCancelTarget();
}
