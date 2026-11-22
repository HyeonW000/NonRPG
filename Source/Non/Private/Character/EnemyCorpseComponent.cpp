#include "Character/EnemyCorpseComponent.h"

#include "Character/EnemyCharacter.h"
#include "Character/NonCharacterBase.h"
#include "Core/NonPlayerController.h"
#include "Data/EnemyDataAsset.h"
#include "Inventory/InventoryComponent.h"
#include "System/PartyComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UEnemyCorpseComponent::UEnemyCorpseComponent() {
  PrimaryComponentTick.bCanEverTick = false;
  SetIsReplicatedByDefault(true);
}

void UEnemyCorpseComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty> &OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);
  DOREPLIFETIME(UEnemyCorpseComponent, bLootAvailable);
}

AEnemyCharacter *UEnemyCorpseComponent::GetOwnerEnemy() const {
  return Cast<AEnemyCharacter>(GetOwner());
}

// ───────────────────────── 페이드 ─────────────────────────

void UEnemyCorpseComponent::InitFadeMaterials() {
  FadeMIDs.Reset();
  AEnemyCharacter *Owner = GetOwnerEnemy();
  USkeletalMeshComponent *Mesh = Owner ? Owner->GetMesh() : nullptr;
  if (!Mesh)
    return;

  const int32 NumMats = Mesh->GetNumMaterials();
  for (int32 i = 0; i < NumMats; ++i) {
    if (UMaterialInterface *Mat = Mesh->GetMaterial(i)) {
      if (UMaterialInstanceDynamic *MID = Mesh->CreateDynamicMaterialInstance(i, Mat)) {
        FadeMIDs.Add(MID);
      }
    }
  }
}

void UEnemyCorpseComponent::SetFadeValue(float Value) {
  for (UMaterialInstanceDynamic *MID : FadeMIDs) {
    if (MID)
      MID->SetScalarParameterValue(FadeParamName, Value);
  }
}

void UEnemyCorpseComponent::PlaySpawnFadeIn(float Duration) {
  AEnemyCharacter *Owner = GetOwnerEnemy();
  if (!Owner)
    return;

  bIsFadingOut = false;
  if (Duration <= 0.01f) {
    SetFadeValue(1.f); // 즉시 표시
    return;
  }

  FadeLen = Duration;
  FadeStartTime = GetWorld()->GetTimeSeconds();
  bFadeActive = true;

  // AI/이동 정지
  if (bPauseAIWhileFading) {
    if (AAIController *AIC = Cast<AAIController>(Owner->GetController())) {
      if (UBrainComponent *Brain = AIC->GetBrainComponent())
        Brain->StopLogic("SpawnFade");
      AIC->StopMovement();
    }
    if (UCharacterMovementComponent *Move = Owner->GetCharacterMovement()) {
      SavedMaxWalkSpeed = Move->MaxWalkSpeed;
      Move->MaxWalkSpeed = 0.f;
    }
  }

  SetFadeValue(0.f); // 투명에서 시작
}

void UEnemyCorpseComponent::TickFade() {
  if (!bFadeActive)
    return;

  const float Elapsed = GetWorld()->GetTimeSeconds() - FadeStartTime;
  const float Alpha = FMath::Clamp(Elapsed / FadeLen, 0.f, 1.f);

  // FadeIn(0->1), FadeOut(1->0)
  SetFadeValue(bIsFadingOut ? (1.f - Alpha) : Alpha);

  if (Alpha < 1.f)
    return;

  bFadeActive = false;
  if (bIsFadingOut) {
    AEnemyCharacter *Owner = GetOwnerEnemy();
    if (Owner && Owner->HasAuthority()) {
      Owner->Destroy(); // 페이드 아웃 끝나면 서버에서 삭제
    }
  } else {
    OnSpawnFadeFinished(); // 페이드 인 끝나면 AI 시작
  }
}

void UEnemyCorpseComponent::OnSpawnFadeFinished() {
  if (!bPauseAIWhileFading || !bAutoResumeAfterFade)
    return;

  AEnemyCharacter *Owner = GetOwnerEnemy();
  if (!Owner)
    return;

  if (AAIController *AIC = Cast<AAIController>(Owner->GetController())) {
    if (UBrainComponent *Brain = AIC->GetBrainComponent())
      Brain->RestartLogic();
  }
  if (UCharacterMovementComponent *Move = Owner->GetCharacterMovement()) {
    Move->MaxWalkSpeed = (SavedMaxWalkSpeed > 0.f) ? SavedMaxWalkSpeed : 350.f;
  }
}

void UEnemyCorpseComponent::Multicast_PlayFadeOut_Implementation(float Duration) {
  if (Duration <= 0.f)
    Duration = 1.0f;

  // 클라이언트에서 MID가 아직 없으면 생성 (안전장치)
  if (FadeMIDs.Num() == 0) {
    InitFadeMaterials();
  }

  FadeLen = Duration;
  FadeStartTime = GetWorld()->GetTimeSeconds();
  bFadeActive = true;
  bIsFadingOut = true;

  SetInteractionOutline(false);
}

// ───────────────────────── 시체 / 루팅 ─────────────────────────

void UEnemyCorpseComponent::EnableCorpseInteraction() {
  AEnemyCharacter *Owner = GetOwnerEnemy();
  if (!Owner)
    return;

  if (USphereComponent *InteractCollision = Owner->GetInteractCollision()) {
    InteractCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    InteractCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
  }
  if (UCapsuleComponent *Capsule = Owner->GetCapsuleComponent()) {
    Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
  }
  if (USkeletalMeshComponent *Mesh = Owner->GetMesh()) {
    Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
  }

  // 루팅 상태/시체 수명은 서버만 관리 (bLootAvailable은 복제, 소멸은 Multicast_PlayFadeOut).
  // 클라이언트에서 돌리면 대상자 목록이 비어 있어 "전원 루팅 완료"로 오판 → 클라이언트 혼자 3초 뒤 페이드아웃.
  if (!Owner->HasAuthority())
    return;

  // 루팅 상태는 처음 한 번만 초기화 (사망 시 + 사망 포즈 고정 시 두 번 호출됨).
  // OnOwnerDied(RollPersonalLoot)에서 받을 것이 없는 대상자를 이미 루팅 완료로 넣어 두었으므로 지우면 안 됨.
  if (!bCorpseStateInitialized) {
    bCorpseStateInitialized = true;
    bLooted = AreAllEligibleLooted(); // 대상자 전원이 받을 것이 없으면 처음부터 루팅 완료
    bLootAvailable = !bLooted;
  }

  // 남은 루팅이 있으면 CorpseLifeTime(120초), 없으면 CorpseLifeTimeAfterLoot 뒤 소멸.
  // 사망 포즈 고정 시 다시 불리면 그 시점부터 다시 계산 (사망 모션 도중 사라지지 않게)
  StartCorpseTimer(bLootAvailable ? CorpseLifeTime : CorpseLifeTimeAfterLoot);
}

void UEnemyCorpseComponent::HandleInteract(ANonCharacterBase *Interactor) {
  AEnemyCharacter *Owner = GetOwnerEnemy();
  if (!Owner || !Owner->IsDead() || !Interactor)
    return;

  ANonPlayerController *PC = Cast<ANonPlayerController>(Interactor->GetController());

  // 루팅 대상자(처치자 / 거리 내 파티원)가 아니면 루팅 불가
  if (!EligibleLooters.Contains(Interactor)) {
    if (PC) {
      PC->Client_OnCorpseLooted(Owner);
      PC->Client_AddSystemMessage(TEXT("[시스템] 이 시체의 전리품 대상자가 아닙니다."));
    }
    return;
  }

  // 개인 루팅 보호: 이미 획득한 플레이어 (받을 것이 없던 대상자 포함)
  if (LootedCharacters.Contains(Interactor)) {
    if (PC) {
      PC->Client_OnCorpseLooted(Owner);
      PC->Client_AddSystemMessage(TEXT("[시스템] 이미 전리품을 획득한 시체입니다."));
    }
    return;
  }

  LootedCharacters.Add(Interactor);

  if (PC) {
    // 루팅 즉시 클라이언트 외곽선 끄기 및 프롬프트 닫기
    PC->Client_OnCorpseLooted(Owner);
  }

  // 사망 시 미리 추첨해 둔 이 플레이어 몫
  TArray<FCorpsePendingLootItem> MyLoot;
  PendingLoot.RemoveAndCopyValue(Interactor, MyLoot);

  UInventoryComponent *Inv = Interactor->GetInventoryComponent();
  if (!Inv) {
    Inv = Interactor->FindComponentByClass<UInventoryComponent>();
  }
  if (!Inv) {
    if (PC) {
      PC->Client_AddSystemMessage(TEXT("[시스템] 플레이어 인벤토리 컴포넌트를 찾을 수 없습니다."));
    }
    return;
  }

  // 미리 추첨된 개인 전리품 지급
  int32 DroppedCount = 0;
  for (const FCorpsePendingLootItem &Loot : MyLoot) {
    int32 OutSlotIndex = -1;
    if (Inv->AddItem(Loot.ItemId, Loot.Quantity, OutSlotIndex)) {
      DroppedCount++;
      if (PC) {
        PC->Client_AddSystemMessage(FString::Printf(TEXT("[전리품] %s x%d 획득!"), *Loot.ItemId.ToString(), Loot.Quantity));
      }
    } else if (PC) {
      PC->Client_AddSystemMessage(FString::Printf(TEXT("[시스템] 아이템 '%s' 지급 실패 (아이템 데이터 테이블에 해당 ID가 있는지 확인 필요)"), *Loot.ItemId.ToString()));
    }
  }

  if (DroppedCount == 0 && PC) {
    PC->Client_AddSystemMessage(TEXT("[전리품] 획득할 수 있는 아이템이 없습니다."));
  }

  // 개인 루팅: 루팅한 플레이어 화면에서만 프롬프트가 사라지고(PC의 LootedCorpses), 다른 파티원은 계속 루팅 가능.
  // 루팅 대상자가 모두 루팅하면 CorpseLifeTimeAfterLoot 뒤 페이드아웃 소멸. 아니면 CorpseLifeTime(120초) 유지.
  if (Owner->HasAuthority() && AreAllEligibleLooted()) {
    bLooted = true;
    bLootAvailable = false;
    StartCorpseTimer(CorpseLifeTimeAfterLoot);
  }
}

bool UEnemyCorpseComponent::AreAllEligibleLooted() const {
  for (const TWeakObjectPtr<ANonCharacterBase> &Looter : EligibleLooters) {
    // 게임을 나간 플레이어(무효)는 기다리지 않음
    if (Looter.IsValid() && !LootedCharacters.Contains(Looter)) {
      return false;
    }
  }
  return true; // 대상자가 없거나(처치자 불명) 모두 루팅함
}

FText UEnemyCorpseComponent::GetInteractLabel() const {
  const AEnemyCharacter *Owner = GetOwnerEnemy();
  if (!Owner || !Owner->IsDead() || !bLootAvailable)
    return FText::GetEmpty();
  return FText::FromString(TEXT("획득하기"));
}

void UEnemyCorpseComponent::SetInteractHighlight(bool bEnable) {
  const AEnemyCharacter *Owner = GetOwnerEnemy();
  SetInteractionOutline(Owner && Owner->IsDead() && bEnable);
}

void UEnemyCorpseComponent::SetInteractionOutline(bool bEnable) {
  AActor *Owner = GetOwner();
  if (!Owner)
    return;

  // 메인 메쉬 포함 모든 메쉬에 1번 스텐실 (프로젝트 전역 아웃라인 채널)
  TArray<UMeshComponent *> MeshComps;
  Owner->GetComponents<UMeshComponent>(MeshComps);
  for (UMeshComponent *MeshComp : MeshComps) {
    if (MeshComp) {
      MeshComp->SetRenderCustomDepth(bEnable);
      MeshComp->SetCustomDepthStencilValue(bEnable ? 1 : 0);
    }
  }
}

void UEnemyCorpseComponent::StartCorpseTimer(float LifeTime) {
  FTimerManager &TM = GetWorld()->GetTimerManager();
  TM.ClearTimer(CorpseRemoveTimerHandle);
  if (LifeTime <= 0.f) {
    OnCorpseExpired();
  } else {
    TM.SetTimer(CorpseRemoveTimerHandle, this, &UEnemyCorpseComponent::OnCorpseExpired, LifeTime, false);
  }
}

void UEnemyCorpseComponent::OnCorpseExpired() {
  // 바로 삭제하지 않고 페이드 아웃 후 소멸
  AEnemyCharacter *Owner = GetOwnerEnemy();
  if (Owner && Owner->HasAuthority()) {
    Multicast_PlayFadeOut(CorpseFadeOutDuration);
  } else {
    Multicast_PlayFadeOut_Implementation(CorpseFadeOutDuration);
  }
}

// ───────────────────────── 보상 ─────────────────────────

void UEnemyCorpseComponent::GatherRewardRecipients(
    ANonCharacterBase *KillerChar, TArray<ANonCharacterBase *> &OutChars,
    TArray<ANonPlayerController *> &OutPCs, bool &bOutIsParty) const {
  OutChars.Reset();
  OutPCs.Reset();
  bOutIsParty = false;

  const AEnemyCharacter *Owner = GetOwnerEnemy();
  if (!Owner || !KillerChar)
    return;

  ANonPlayerController *KillerPC = Cast<ANonPlayerController>(KillerChar->GetController());
  UPartyComponent *PartyComp = KillerPC ? KillerPC->GetPartyComponent() : nullptr;

  if (PartyComp && PartyComp->IsInParty()) {
    bOutIsParty = true;
    const FVector MonsterLoc = Owner->GetActorLocation();
    for (const FPartyMemberInfo &Member : PartyComp->GetPartyData().Members) {
      ANonCharacterBase *MemberChar = Member.Character.Get();
      if (MemberChar && !MemberChar->IsDead() &&
          FVector::Dist(MonsterLoc, MemberChar->GetActorLocation()) <= PartyExpShareRadius) {
        OutChars.Add(MemberChar);
        OutPCs.Add(Member.PlayerController.Get());
      }
    }
  }

  // 솔로이거나 거리 내 파티원이 아무도 없으면 처치자만
  if (OutChars.Num() == 0) {
    OutChars.Add(KillerChar);
    OutPCs.Add(KillerPC);
  }
}

void UEnemyCorpseComponent::OnOwnerDied(ANonCharacterBase *KillerChar, float ExpReward) {
  AEnemyCharacter *Owner = GetOwnerEnemy();
  if (!Owner || !Owner->HasAuthority())
    return;

  // 루팅 대상자 확정 (사망 순간 기준, 경험치 분배 대상과 동일)
  TArray<ANonCharacterBase *> Chars;
  TArray<ANonPlayerController *> PCs;
  bool bIsParty = false;
  GatherRewardRecipients(KillerChar, Chars, PCs, bIsParty);

  EligibleLooters.Reset();
  for (ANonCharacterBase *Char : Chars) {
    EligibleLooters.Add(Char);
  }

  // 대상자별 전리품 미리 추첨 + 받을 것이 없는 사람/비대상자 화면에서 상호작용 차단
  RollPersonalLoot();

  if (ExpReward > 0.f) {
    DistributeExp(KillerChar, ExpReward);
  }
}

void UEnemyCorpseComponent::RollPersonalLoot() {
  AEnemyCharacter *Owner = GetOwnerEnemy();
  if (!Owner || !Owner->HasAuthority())
    return;

  PendingLoot.Reset();
  const UEnemyDataAsset *EnemyData = Owner->EnemyData;
  if (!EnemyData) {
    UE_LOG(LogTemp, Warning, TEXT("[Loot] %s: EnemyData가 없어 전리품이 없습니다."), *Owner->GetName());
  }

  // 1) 대상자마다 독립 추첨 (개인 루팅)
  for (const TWeakObjectPtr<ANonCharacterBase> &Looter : EligibleLooters) {
    if (!Looter.IsValid())
      continue;

    TArray<FCorpsePendingLootItem> Items;
    if (EnemyData) {
      for (const FEnemyDropItem &Drop : EnemyData->DropList) {
        if (Drop.ItemId.IsNone() || FMath::FRand() > Drop.DropChance)
          continue;
        const int32 Quantity = FMath::RandRange(Drop.MinCount, Drop.MaxCount);
        if (Quantity <= 0)
          continue;
        Items.Add({Drop.ItemId, Quantity});
      }
    }

    UE_LOG(LogTemp, Log, TEXT("[Loot] %s → %s: 전리품 %d종%s"), *Owner->GetName(), *Looter->GetName(),
           Items.Num(), Items.Num() == 0 ? TEXT(" (없음 → 외곽선/루팅 차단)") : TEXT(""));

    if (Items.Num() > 0) {
      PendingLoot.Add(Looter, MoveTemp(Items));
    } else {
      // 받을 것이 없음 → 처음부터 루팅 완료 처리 (외곽선/획득하기 팝업/상호작용 없음)
      LootedCharacters.AddUnique(Looter);
      if (ANonPlayerController *PC = Cast<ANonPlayerController>(Looter->GetController())) {
        PC->Client_OnCorpseLooted(Owner);
      }
    }
  }

  // 2) 비대상자(처치에 관여하지 않은 플레이어) 화면에서도 상호작용 차단
  for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) {
    ANonPlayerController *PC = Cast<ANonPlayerController>(It->Get());
    if (!PC)
      continue;
    ANonCharacterBase *Char = Cast<ANonCharacterBase>(PC->GetPawn());
    if (!Char || !EligibleLooters.Contains(Char)) {
      PC->Client_OnCorpseLooted(Owner);
    }
  }
}

void UEnemyCorpseComponent::DistributeExp(ANonCharacterBase *KillerChar, float ExpReward) {
  AEnemyCharacter *Owner = GetOwnerEnemy();
  if (!Owner || !Owner->HasAuthority() || ExpReward <= 0.f || !KillerChar)
    return;

  auto GiveExp = [](ANonCharacterBase *Char, ANonPlayerController *PC, float Amount) {
    Char->GainExp(Amount);
    if (PC) {
      PC->Client_AddSystemMessage(FString::Printf(TEXT("경험치 %.0f 을 획득했습니다."), Amount));
    }
  };

  TArray<ANonCharacterBase *> EligibleMembers;
  TArray<ANonPlayerController *> EligiblePCs;
  bool bIsParty = false;
  GatherRewardRecipients(KillerChar, EligibleMembers, EligiblePCs, bIsParty);

  const int32 Count = EligibleMembers.Num();
  if (Count <= 1) {
    // 솔로, 거리 내 1명, 또는 거리 내 아무도 없어 처치자만: 보너스 없이 100%
    GiveExp(EligibleMembers[0], EligiblePCs[0], ExpReward);
    return;
  }

  // 2인 이상: 인원수당 보너스 추가 후 균등 분배 (2인 110%, 3인 120%, 4인 130%)
  const float TotalExpWithBonus = ExpReward * (1.0f + (Count - 1) * PartyExpBonusPerMember);
  const float ExpPerMember = TotalExpWithBonus / static_cast<float>(Count);
  for (int32 i = 0; i < Count; ++i) {
    GiveExp(EligibleMembers[i], EligiblePCs[i], ExpPerMember);
  }
}
