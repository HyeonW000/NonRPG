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

  bLootAvailable = true;
  bLooted = false;
  LootedCharacters.Empty();

  StartCorpseTimer(CorpseLifeTime);
}

void UEnemyCorpseComponent::HandleInteract(ANonCharacterBase *Interactor) {
  AEnemyCharacter *Owner = GetOwnerEnemy();
  if (!Owner || !Owner->IsDead() || !Interactor)
    return;

  ANonPlayerController *PC = Cast<ANonPlayerController>(Interactor->GetController());

  // 개인 루팅 보호: 이미 획득한 플레이어
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

  const UEnemyDataAsset *EnemyData = Owner->EnemyData;
  if (!EnemyData) {
    if (PC) {
      PC->Client_AddSystemMessage(TEXT("[시스템] 이 몬스터에 EnemyData(데이터 에셋)가 설정되지 않아 전리품이 없습니다."));
    }
    return;
  }

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

  // 개인별 독립 드롭 추첨·지급
  int32 DroppedCount = 0;
  for (const FEnemyDropItem &Drop : EnemyData->DropList) {
    if (FMath::FRand() > Drop.DropChance)
      continue;

    const int32 Quantity = FMath::RandRange(Drop.MinCount, Drop.MaxCount);
    if (Quantity <= 0)
      continue;

    int32 OutSlotIndex = -1;
    if (Inv->AddItem(Drop.ItemId, Quantity, OutSlotIndex)) {
      DroppedCount++;
      if (PC) {
        PC->Client_AddSystemMessage(FString::Printf(TEXT("[전리품] %s x%d 획득!"), *Drop.ItemId.ToString(), Quantity));
      }
    } else if (PC) {
      PC->Client_AddSystemMessage(FString::Printf(TEXT("[시스템] 아이템 '%s' 지급 실패 (아이템 데이터 테이블에 해당 ID가 있는지 확인 필요)"), *Drop.ItemId.ToString()));
    }
  }

  if (DroppedCount == 0 && PC) {
    PC->Client_AddSystemMessage(TEXT("[전리품] 획득할 수 있는 아이템이 없습니다."));
  }

  // 루팅 완료 → 프롬프트 비활성화, 정해진 시간 뒤 페이드아웃 소멸
  bLooted = true;
  bLootAvailable = false;

  if (Owner->HasAuthority()) {
    StartCorpseTimer(CorpseLifeTimeAfterLoot);
  }
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

void UEnemyCorpseComponent::DistributeExp(ANonCharacterBase *KillerChar, float ExpReward) {
  AEnemyCharacter *Owner = GetOwnerEnemy();
  if (!Owner || !Owner->HasAuthority() || ExpReward <= 0.f || !KillerChar)
    return;

  ANonPlayerController *KillerPC = Cast<ANonPlayerController>(KillerChar->GetController());
  UPartyComponent *PartyComp = KillerPC ? KillerPC->GetPartyComponent() : nullptr;

  auto GiveExp = [](ANonCharacterBase *Char, ANonPlayerController *PC, float Amount) {
    Char->GainExp(Amount);
    if (PC) {
      PC->Client_AddSystemMessage(FString::Printf(TEXT("경험치 %.0f 을 획득했습니다."), Amount));
    }
  };

  // 솔로 / 파티 미소속
  if (!PartyComp || !PartyComp->IsInParty()) {
    GiveExp(KillerChar, KillerPC, ExpReward);
    return;
  }

  // 파티: 거리 제한 이내의 살아 있는 파티원
  const FPartyData &PartyData = PartyComp->GetPartyData();
  TArray<ANonCharacterBase *> EligibleMembers;
  TArray<ANonPlayerController *> EligiblePCs;
  const FVector MonsterLoc = Owner->GetActorLocation();

  for (const FPartyMemberInfo &Member : PartyData.Members) {
    ANonCharacterBase *MemberChar = Member.Character.Get();
    if (MemberChar && !MemberChar->IsDead() &&
        FVector::Dist(MonsterLoc, MemberChar->GetActorLocation()) <= PartyExpShareRadius) {
      EligibleMembers.Add(MemberChar);
      EligiblePCs.Add(Member.PlayerController.Get());
    }
  }

  const int32 Count = EligibleMembers.Num();
  if (Count <= 0) {
    // 거리 내 아무도 없으면 마지막 타격자에게 전액 (기존 동작: 메시지 없음)
    KillerChar->GainExp(ExpReward);
    return;
  }

  if (Count == 1) {
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
