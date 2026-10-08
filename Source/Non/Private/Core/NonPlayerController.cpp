#include "Core/NonPlayerController.h"
#include "Core/NonGameMode.h"       // [New]
#include "Kismet/GameplayStatics.h" // [New]
#include "Net/UnrealNetwork.h"      // [New]
#include "System/NonGameInstance.h" // [New]
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "Character/NonCharacterBase.h"
#include "Core/NonUIManagerComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Interaction/NonInteractableInterface.h"
#include "UI/CharacterCreationWidget.h" // [New]
#include "UI/CharacterSelectWidget.h"
#include "UI/QuickSlot/QuickSlotManager.h"
#include "UI/InGameHUD.h"
#include "UI/Chat/ChatBoxWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryItem.h"
#include "UI/PlayerInteractionMenuWidget.h"
#include "UI/InteractionRequestWidget.h"
#include "System/PartyComponent.h"
#include "System/TradeComponent.h"

// 상호작용용 트레이스 채널 (프로젝트 세팅에서 만든 Interact 채널이
// GameTraceChannel1 이라는 가정)
static const ECollisionChannel InteractChannel =
    ECollisionChannel::ECC_GameTraceChannel1;
// IMC에서 액션을 이름으로 찾아오는 헬퍼 (UE5.5)
static const UInputAction *FindActionInIMC(const UInputMappingContext *IMC,
                                           FName ActionName) {
  if (!IMC)
    return nullptr;

  // 반환은 TConstArrayView<FEnhancedActionKeyMapping>
  const TConstArrayView<FEnhancedActionKeyMapping> Mappings =
      IMC->GetMappings();

  for (const FEnhancedActionKeyMapping &Map : Mappings) {
    if (Map.Action && Map.Action->GetFName() == ActionName) {
      return Map.Action
          .Get(); // TObjectPtr<const UInputAction> → const UInputAction*
    }
  }
  return nullptr;
}

// 이름으로 찾아 바인딩(없으면 로그만)
template <typename UserClass, typename FuncType>
static void BindIfFound(UEnhancedInputComponent *EIC,
                        const UInputMappingContext *IMC,
                        const TCHAR *ActionName, ETriggerEvent Event,
                        UserClass *Obj, FuncType Func) {
  if (!EIC || !IMC)
    return;

  if (const UInputAction *IA = FindActionInIMC(IMC, FName(ActionName))) {
    EIC->BindAction(IA, Event, Obj, Func);
  } else {
  }
}

ANonPlayerController::ANonPlayerController() {
  bShowMouseCursor = false;

  PartyComponent = CreateDefaultSubobject<UPartyComponent>(TEXT("PartyComponent"));
  TradeComponent = CreateDefaultSubobject<UTradeComponent>(TEXT("TradeComponent"));
}

UPartyComponent* ANonPlayerController::GetPartyComponent()
{
  if (!PartyComponent)
  {
    PartyComponent = FindComponentByClass<UPartyComponent>();
    if (!PartyComponent)
    {
      PartyComponent = NewObject<UPartyComponent>(this, TEXT("PartyComponent_Auto"));
      PartyComponent->RegisterComponent();
    }
  }
  return PartyComponent;
}

UTradeComponent* ANonPlayerController::GetTradeComponent()
{
  if (!TradeComponent)
  {
    TradeComponent = FindComponentByClass<UTradeComponent>();
    if (!TradeComponent)
    {
      TradeComponent = NewObject<UTradeComponent>(this, TEXT("TradeComponent_Auto"));
      TradeComponent->RegisterComponent();
    }
  }
  return TradeComponent;
}

void ANonPlayerController::BeginPlay() {
  Super::BeginPlay();

  if (ULocalPlayer *LP = GetLocalPlayer()) {
    if (UEnhancedInputLocalPlayerSubsystem *Subsys =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
                LP)) {
      if (IMC_Default)
        Subsys->AddMappingContext(IMC_Default, 0);
      if (IMC_QuickSlots)
        Subsys->AddMappingContext(IMC_QuickSlots, 1);
    }
  }

  if (FSlateApplication::IsInitialized()) {
    FSlateApplication::Get().SetDragTriggerDistance(1);
  }

  // [New] 로비(UI Only)에서 넘어왔을 때를 대비해 강제로 Game Only로 설정
  if (IsLocalController()) {
    // 내 캐릭터가 이미 있는지 확인
    ANonCharacterBase *MyChar = Cast<ANonCharacterBase>(GetPawn());

    if (MyChar) {
      // 캐릭터가 있으면 게임 시작
      FInputModeGameOnly GameMode;
      SetInputMode(GameMode);
      bShowMouseCursor = false;
      bEnableClickEvents = false;
      bEnableMouseOverEvents = false;
    } else {
      // 캐릭터가 없어도, 로비에서 넘어와서 스폰 대기 중인지 확인
      int32 PendingSlotIndex = -1;
      if (UNonGameInstance *GI = Cast<UNonGameInstance>(GetGameInstance())) {
        // 인덱스가 유효한지 (-1이 아닌지) 확인
        if (GI->CurrentSlotIndex >= 0) {
          PendingSlotIndex = GI->CurrentSlotIndex;
        }
      }

      if (PendingSlotIndex >= 0) {
        // 서버에 스폰 요청
        ServerSpawnCharacter(PendingSlotIndex);
      } else {
        // 캐릭터도 없고, 대기 중인 슬롯도 없음 (예: 파티원 따라 강제 이동된
        // 경우)

        UWorld *World = GetWorld();
        FString CurrentMapName = (World ? World->GetMapName() : TEXT(""));
        CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);

        if (CurrentMapName.Contains(TEXT("Lobby")) ||
            CurrentMapName.Contains(TEXT("Title"))) {
          // 로비에서는 타이틀 화면
          ShowTitleUI();
        } else {
          // 이미 게임 맵에 들어와 있다면 -> 바로 임시 캐릭터 스폰 (테스트용)
          // GoToCharacterSelect(); // 원래 로직: 타이틀 스킵 후 선택창

          if (UNonGameInstance *GI =
                  Cast<UNonGameInstance>(GetGameInstance())) {
            GI->CurrentSlotIndex =
                0; // 혹시라도 다른 로직이 참조할 수 있으므로 GI에 박아둠
          }
          ServerSpawnCharacter(0);
        }
      }
    }
  }
}

void ANonPlayerController::OnPossess(APawn *InPawn) {
  Super::OnPossess(InPawn);
  // Server: SetPawn이 호출되긴 하지만, 명시적으로 여기서도 갱신
  CachedChar = Cast<ANonCharacterBase>(InPawn);
  CachedQuick =
      (InPawn ? InPawn->FindComponentByClass<UQuickSlotManager>() : nullptr);

  // [New] 빙의가 완료된 시점에 HUD 초기화 및 갱신을 보장
  if (InPawn && IsLocalController()) {
    if (UNonUIManagerComponent *UIMan =
            InPawn->FindComponentByClass<UNonUIManagerComponent>()) {
      UIMan->InitHUD();
      UIMan->RefreshHUDState(); // 스탯/직업 즉시 갱신
    }
  }
}

void ANonPlayerController::SetPawn(APawn *InPawn) {
  Super::SetPawn(InPawn);

  // [Fix] Client/Server 모두 폰이 변경될 때 캐싱 갱신
  CachedChar = Cast<ANonCharacterBase>(InPawn);
  CachedQuick =
      (InPawn ? InPawn->FindComponentByClass<UQuickSlotManager>() : nullptr);

  // [New] 로컬 클라이언트가 새 폰에 빙의했을 때, 자신의 장비/위치 정보를 서버에 알림 (옵션 활성화 시)
  if (bEnableAutoLoadOnSpawn && InPawn && IsLocalController()) {
    int32 SlotIndex = -1;
    if (UNonGameInstance *GI = Cast<UNonGameInstance>(GetGameInstance())) {
      SlotIndex = GI->CurrentSlotIndex;
    }

    if (SlotIndex >= 0) {
      FString SlotName = FString::Printf(TEXT("Slot%d"), SlotIndex);
      if (UNonGameInstance *GI = Cast<UNonGameInstance>(GetGameInstance())) {
        SlotName = GI->GetSaveSlotName(SlotIndex);
      }

      if (UGameplayStatics::DoesSaveGameExist(SlotName, 0)) {
        if (UNonSaveGame *Data = Cast<UNonSaveGame>(
                UGameplayStatics::LoadGameFromSlot(SlotName, 0))) {
          // 장비 정보 서버로 전송
          if (Data->EquippedItems.Num() > 0)
          {
            ServerSyncEquipment(Data->EquippedItems);
          }

          // [Multiplayer Fix] 인벤토리 정보를 서버로 전송하여 서버 측 인벤토리 동기화!
          if (Data->InventoryItems.Num() > 0)
          {
            ServerSyncInventory(Data->InventoryItems);
          }

          // 위치 정보 서버로 전송
          if (!Data->PlayerTransform.Equals(FTransform::Identity))
          {
            ServerSyncTransform(Data->PlayerTransform);
          }
        }
      }
    }
  }
}

void ANonPlayerController::ServerSyncEquipment_Implementation(
    const TArray<FEquipmentSaveData> &EquipmentData) {
  if (ANonCharacterBase *Char = Cast<ANonCharacterBase>(GetPawn())) {
    if (UEquipmentComponent *EquipComp =
            Char->FindComponentByClass<UEquipmentComponent>()) {
      EquipComp->RestoreEquippedItemsFromSave(EquipmentData);
    }
  }
}

void ANonPlayerController::ServerSyncInventory_Implementation(
    const TArray<FInventorySaveData> &InventoryData) {
  if (ANonCharacterBase *Char = Cast<ANonCharacterBase>(GetPawn())) {
    if (UInventoryComponent *InvComp =
            Char->FindComponentByClass<UInventoryComponent>()) {
      InvComp->RestoreItemsFromSave(InventoryData);
    }
  }
}

void ANonPlayerController::Server_CheatAddItems_Implementation(
    const TArray<FName> &ItemIds, int32 QuantityPerItem) {
  if (ANonCharacterBase *Char = Cast<ANonCharacterBase>(GetPawn())) {
    if (UInventoryComponent *InvComp =
            Char->FindComponentByClass<UInventoryComponent>()) {
      InvComp->AddMultipleItems(ItemIds, QuantityPerItem);
    }
  }
}

void ANonPlayerController::ServerSyncTransform_Implementation(
    const FTransform &Transform) {
  if (APawn *MyPawn = GetPawn()) {
    // 안전하게 살짝 위에서 생성 (+40cm)
    FTransform NewTransform = Transform;
    FVector NewLoc = NewTransform.GetLocation();
    NewLoc.Z += 40.0f;
    NewTransform.SetLocation(NewLoc);

    // 텔레포트
    MyPawn->SetActorTransform(NewTransform, false, nullptr,
                              ETeleportType::TeleportPhysics);
  }
}

TSharedPtr<SViewport>
ANonPlayerController::GetGameViewportSViewport(UWorld *World) {
  if (!World)
    return nullptr;
  if (UGameViewportClient *GVC = World->GetGameViewport()) {
    return GVC->GetGameViewportWidget();
  }
  return nullptr;
}

void ANonPlayerController::PlayerTick(float DeltaTime) {
  Super::PlayerTick(DeltaTime);

  if (FSlateApplication::IsInitialized() &&
      FSlateApplication::Get().IsDragDropping()) {
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetWidgetToFocus(nullptr);
    SetInputMode(Mode);

    if (TSharedPtr<SViewport> VP = GetGameViewportSViewport(GetWorld())) {
      const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
      FSlateApplication::Get().SetUserFocus(
          UserIdx, StaticCastSharedPtr<SWidget>(VP), EFocusCause::SetDirectly);
      FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
    }
  }

  UpdateInteractFocus(DeltaTime);

  // [New] 대화 쿨다운 갱신
  if (DialogueEndCooldown > 0.f) {
    DialogueEndCooldown -= DeltaTime;
  }

  // 🏃 [Auto-Run] 자동 달리기 프레임 처리
  if (bIsAutoRunning) {
    ProcessAutoRun();
  }
}

void ANonPlayerController::SetupInputComponent() {
  Super::SetupInputComponent();

  // ── [전역 단축키] 파티/거래/결투 알림 팝업 수락(Y) / 거절(N) 바인딩 ──
  if (InputComponent)
  {
    InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &ANonPlayerController::OnHotkeyAcceptRequest);
    InputComponent->BindKey(EKeys::N, IE_Pressed, this, &ANonPlayerController::OnHotkeyDeclineRequest);
  }

  UEnhancedInputComponent *EIC = Cast<UEnhancedInputComponent>(InputComponent);
  if (!EIC)
    return;

  // 기본 IMC에서 자동 바인딩 (이전 방식: 이름 검색) -> [Change] 명시적 Property
  // 사용 BindIfFound(EIC, IMC_Default, TEXT("IA_Move"),
  // ETriggerEvent::Triggered, this, &ThisClass::OnMove); // 캐릭터에서 처리함
  // BindIfFound(EIC, IMC_Default, TEXT("IA_Look"), ETriggerEvent::Triggered,
  // this, &ThisClass::OnLook); // 캐릭터에서 처리함

  // 캐릭터 Movement는 캐릭터(SetupPlayerInputComponent)에서 직접 바인딩하므로
  // 여기서는 생략 능.
  // -> [Refactor] 사용자의 요청으로 Controller로 통합.

  // Movement & Combat
  if (IA_Move)
    EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this,
                    &ThisClass::OnMove);
  if (IA_Look)
    EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this,
                    &ThisClass::OnLook);

  if (IA_Jump) {
    EIC->BindAction(IA_Jump, ETriggerEvent::Started, this,
                    &ThisClass::OnJumpStart);
    EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this,
                    &ThisClass::OnJumpStop);
  }

  if (IA_Attack)
    EIC->BindAction(IA_Attack, ETriggerEvent::Started, this,
                    &ThisClass::OnAttack);
  if (IA_Dodge)
    EIC->BindAction(IA_Dodge, ETriggerEvent::Started, this,
                    &ThisClass::OnDodge);

  if (IA_Guard) {
    EIC->BindAction(IA_Guard, ETriggerEvent::Started, this,
                    &ThisClass::OnGuardPressed);
    EIC->BindAction(IA_Guard, ETriggerEvent::Completed, this,
                    &ThisClass::OnGuardReleased);
    EIC->BindAction(IA_Guard, ETriggerEvent::Canceled, this,
                    &ThisClass::OnGuardReleased);
  }

  // UI 관련 기능은 여기서 바인딩.
  if (IA_Inventory) {
    EIC->BindAction(IA_Inventory, ETriggerEvent::Started, this,
                    &ThisClass::OnInventory);
  }

  if (IA_SkillWindow)
    EIC->BindAction(IA_SkillWindow, ETriggerEvent::Started, this,
                    &ThisClass::OnToggleSkillWindow);
  if (IA_CharacterWindow)
    EIC->BindAction(IA_CharacterWindow, ETriggerEvent::Started, this,
                    &ThisClass::OnToggleCharacterWindow);
  if (IA_Interact)
    EIC->BindAction(IA_Interact, ETriggerEvent::Started, this,
                    &ANonPlayerController::OnInteract);
  if (IA_ESC)
    EIC->BindAction(IA_ESC, ETriggerEvent::Started, this,
                    &ANonPlayerController::OnEsc);
  if (IA_ToggleArmed)
    EIC->BindAction(IA_ToggleArmed, ETriggerEvent::Started, this,
                    &ThisClass::OnToggleArmed);
  if (IA_CursorToggle)
    EIC->BindAction(IA_CursorToggle, ETriggerEvent::Started, this,
                    &ThisClass::ToggleCursorLook);
  if (IA_Zoom)
    EIC->BindAction(IA_Zoom, ETriggerEvent::Triggered, this,
                    &ThisClass::OnZoom);
  if (IA_Chat)
    EIC->BindAction(IA_Chat, ETriggerEvent::Started, this,
                    &ThisClass::OnToggleChat);
  if (IA_AutoRun)
    EIC->BindAction(IA_AutoRun, ETriggerEvent::Started, this,
                    &ThisClass::ToggleAutoRun);

  // 퀵슬롯 IMC가 있으면 자동 바인딩 (여전히 이름 규칙 사용 - 퀵슬롯은 1~0
  // 규칙적이므로 유지해도 됨) 혹은 개별 Property로 뺄 수도 있지만, 슬롯이
  // 많으므로 기존 방식 유지 또는 리팩토링 고려. 일단 기존 코드가 IMC_Default에
  // 의존하던 부분만 제거.

  // QuickSlot은 예외적으로 "IA_QS_1" 등 이름을 그대로 씀 (필요하면 이것도
  // Property화 가능)
  if (IMC_QuickSlots) {
    BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_1"), ETriggerEvent::Started,
                this, &ThisClass::OnQS1);
    BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_2"), ETriggerEvent::Started,
                this, &ThisClass::OnQS2);
    BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_3"), ETriggerEvent::Started,
                this, &ThisClass::OnQS3);
    BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_4"), ETriggerEvent::Started,
                this, &ThisClass::OnQS4);
    BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_5"), ETriggerEvent::Started,
                this, &ThisClass::OnQS5);
    BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_6"), ETriggerEvent::Started,
                this, &ThisClass::OnQS6);
    BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_7"), ETriggerEvent::Started,
                this, &ThisClass::OnQS7);
    BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_8"), ETriggerEvent::Started,
                this, &ThisClass::OnQS8);
    BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_9"), ETriggerEvent::Started,
                this, &ThisClass::OnQS9);
    BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_0"), ETriggerEvent::Started,
                this, &ThisClass::OnQS0);
  }
}

// QuickSlot
void ANonPlayerController::OnQS0(const FInputActionInstance &) {
  HandleQuickSlot(10);
}
void ANonPlayerController::OnQS1(const FInputActionInstance &) {
  HandleQuickSlot(1);
}
void ANonPlayerController::OnQS2(const FInputActionInstance &) {
  HandleQuickSlot(2);
}
void ANonPlayerController::OnQS3(const FInputActionInstance &) {
  HandleQuickSlot(3);
}
void ANonPlayerController::OnQS4(const FInputActionInstance &) {
  HandleQuickSlot(4);
}
void ANonPlayerController::OnQS5(const FInputActionInstance &) {
  HandleQuickSlot(5);
}
void ANonPlayerController::OnQS6(const FInputActionInstance &) {
  HandleQuickSlot(6);
}
void ANonPlayerController::OnQS7(const FInputActionInstance &) {
  HandleQuickSlot(7);
}
void ANonPlayerController::OnQS8(const FInputActionInstance &) {
  HandleQuickSlot(8);
}
void ANonPlayerController::OnQS9(const FInputActionInstance &) {
  HandleQuickSlot(9);
}

void ANonPlayerController::ToggleAutoRun() {
  SetAutoRunning(!bIsAutoRunning);
}

void ANonPlayerController::SetAutoRunning(bool bEnable) {
  if (bEnable) {
    if (!CachedChar || CachedChar->IsDead()) {
      bIsAutoRunning = false;
      return;
    }
    if (UNonUIManagerComponent *UIMan = CachedChar->FindComponentByClass<UNonUIManagerComponent>()) {
      if (UIMan->IsDialogueActive() || UIMan->IsMerchantShopOpen()) {
        bIsAutoRunning = false;
        return;
      }
    }
    bIsAutoRunning = true;
  } else {
    bIsAutoRunning = false;
  }
}

void ANonPlayerController::ProcessAutoRun() {
  if (!CachedChar || CachedChar->IsDead()) {
    bIsAutoRunning = false;
    return;
  }

  if (UNonUIManagerComponent *UIMan = CachedChar->FindComponentByClass<UNonUIManagerComponent>()) {
    if (UIMan->IsDialogueActive() || UIMan->IsMerchantShopOpen()) {
      bIsAutoRunning = false;
      return;
    }
  }

  // 카메라가 바라보는 전방(Forward) 방향으로 1.0 입력 공급
  const FRotator Rotation = GetControlRotation();
  const FRotator YawRotation(0, Rotation.Yaw, 0);
  const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

  CachedChar->AddMovementInput(Forward, 1.0f);
}

void ANonPlayerController::OnMove(const FInputActionValue &Value) {
  const FVector2D MoveVal = Value.Get<FVector2D>();

  // 🏃 자동 달리기 중 이동 입력 처리:
  // 전/후 입력(W, S: MoveVal.Y)이 감지되면 자동 달리기를 해제하지만,
  // 좌/우 입력(A, D: MoveVal.X)은 자동 달리기를 풀지 않고 방향 조절을 허용합니다!
  if (bIsAutoRunning && FMath::Abs(MoveVal.Y) > 0.1f) {
    SetAutoRunning(false);
  }

  if (CachedChar)
    CachedChar->MoveInput(Value);
}
void ANonPlayerController::OnLook(const FInputActionValue &Value) {
  if (CachedChar)
    CachedChar->LookInput(Value);
}
void ANonPlayerController::OnZoom(const FInputActionValue &Value) {
  // 마우스 커서가 UI 위에 있다면 줌을 무시합니다 (인벤토리 스크롤 등과 겹치는 문제 방지)
  if (bShowMouseCursor) {
    bool bOverUI = false;
    if (APawn *P = GetPawn()) {
      if (UNonUIManagerComponent *UIMan =
              P->FindComponentByClass<UNonUIManagerComponent>()) {
        bOverUI = UIMan->IsCursorOverUI();
      }
    }
    
    if (bOverUI) {
      return;
    }
  }

  if (CachedChar)
    CachedChar->CameraZoom(Value.Get<float>());
}
void ANonPlayerController::OnJumpStart(const FInputActionValue & /*Value*/) {
  if (!CachedChar)
    return;

  // 캐릭터에서 ASC 꺼내기
  UAbilitySystemComponent *ASC = nullptr;
  if (IAbilitySystemInterface *ASI =
          Cast<IAbilitySystemInterface>(CachedChar)) {
    ASC = ASI->GetAbilitySystemComponent();
  }

  // 태그 있으면 점프 막기
  if (ASC) {
    static const FGameplayTag DodgeTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Dodge"), false);

    static const FGameplayTag AttackTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Attack"), false);

    static const FGameplayTag ComboActiveTag =
        FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo"), false);

    static const FGameplayTag SkillTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Skill"), false);

    static const FGameplayTag GuardTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Guard"), false);

    static const FGameplayTag KnockdownTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Knockdown"), false);

    static const FGameplayTag HitTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Hit"), false);

    static const FGameplayTag CCTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.CrowdControl"), false);

    static const FGameplayTag DeadTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false);

    if (ASC->HasMatchingGameplayTag(DodgeTag) ||
        ASC->HasMatchingGameplayTag(AttackTag) ||
        ASC->HasMatchingGameplayTag(ComboActiveTag) ||
        ASC->HasMatchingGameplayTag(SkillTag) ||
        ASC->HasMatchingGameplayTag(GuardTag) ||
        ASC->HasMatchingGameplayTag(KnockdownTag) ||
        ASC->HasMatchingGameplayTag(HitTag) ||
        ASC->HasMatchingGameplayTag(CCTag) ||
        ASC->HasMatchingGameplayTag(DeadTag)) {
      return; // 점프 안 함
    }
  }

  // 캐릭터 점프 가능 여부 최종 확인 후 점프
  if (CachedChar->CanJump()) {
    CachedChar->Jump();
  }
}

void ANonPlayerController::OnJumpStop(const FInputActionValue & /*Value*/) {
  if (CachedChar)
    CachedChar->StopJumping();
}

void ANonPlayerController::OnInteract(
    const FInputActionInstance & /*Instance*/) {
  // ── [F 키 토글] 이미 플레이어 상호작용 메뉴가 열려 있다면 F 키 입력 시 즉시 닫기 ──
  if (PlayerInteractionMenuWidget && PlayerInteractionMenuWidget->IsInViewport() &&
      PlayerInteractionMenuWidget->GetVisibility() == ESlateVisibility::Visible)
  {
      PlayerInteractionMenuWidget->CloseMenu();
      return;
  }

  if (!CachedChar)
    return;

  // ASC 꺼내기
  UAbilitySystemComponent *ASC = nullptr;
  if (IAbilitySystemInterface *ASI =
          Cast<IAbilitySystemInterface>(CachedChar)) {
    ASC = ASI->GetAbilitySystemComponent();
  }

  if (ASC) {
    static const FGameplayTag DodgeTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Dodge"));
    static const FGameplayTag AttackTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Attack"));
    static const FGameplayTag SkillTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Skill"));
    static const FGameplayTag GuardTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Guard"));
    static const FGameplayTag ComboTag =
        FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo"));

    if (ASC->HasMatchingGameplayTag(DodgeTag) ||
        ASC->HasMatchingGameplayTag(AttackTag) ||
        ASC->HasMatchingGameplayTag(SkillTag) ||
        ASC->HasMatchingGameplayTag(GuardTag) ||
        ASC->HasMatchingGameplayTag(ComboTag)) {
      // 전투 중이면 상호작용 무시
      return;
    }
  }

  // ==== 여기부터 캡슐 스윕 대신 포커스 타겟 사용 ====

  AActor *Target = CurrentInteractTarget.Get();
  if (!Target) {
    // 현재 바라보는 상호작용 대상이 없으면 아무 것도 안 함
    return;
  }

  // 혹시 모를 안전 체크 (인터페이스 달려있는지)
  if (!Target->GetClass()->ImplementsInterface(
          UNonInteractableInterface::StaticClass())) {
    return;
  }

  // 실제 상호작용 호출
  INonInteractableInterface::Execute_Interact(Target, CachedChar);
}

void ANonPlayerController::OnAttack(const FInputActionValue &Value) {
  if (bShowMouseCursor) {
    bool bOverUI = false;
    if (APawn *P = GetPawn()) {
      if (UNonUIManagerComponent *UIMan =
              P->FindComponentByClass<UNonUIManagerComponent>()) {
        bOverUI = UIMan->IsCursorOverUI();
      }
    }

    if (!bOverUI) {
      // [New Fix] 마우스 클릭(Pressed) 처리 도중에 즉각 SetInputMode를 하면 Slate 입력 이벤트 스트림이 꼬여서 
      // 화면 회전이 먹통이 되는 고질적 문제를 해결하기 위해, 클릭이 완결된 다음 프레임(0.02초 뒤)에 비동기로 안전하게 인풋을 복구합니다.
      if (UWorld* World = GetWorld()) {
        FTimerHandle TempHandle;
        TWeakObjectPtr<ANonPlayerController> WeakThis(this);
        World->GetTimerManager().SetTimer(TempHandle, [WeakThis]() {
          if (WeakThis.IsValid()) {
            WeakThis->SetShowMouseCursor(false);
            FInputModeGameOnly GameOnly;
            WeakThis->SetInputMode(GameOnly);
            WeakThis->bEnableClickEvents = false;
            WeakThis->bEnableMouseOverEvents = false;

            if (UWorld* InnerWorld = WeakThis->GetWorld()) {
              if (UGameViewportClient* ViewportClient = InnerWorld->GetGameViewport()) {
                ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
              }
            }
          }
        }, 0.02f, false);
      }
    }
    return;
  }

  if (CachedChar) {
    CachedChar->HandleAttackInput(Value);

    // [New Fix] 평상시(UI가 없는 일반 필드 전투) 공격 클릭 시에도 마우스 포커스가 미세하게 유실되어 시점이 굳어버리는 고질적 문제를 방지하기 위해,
    // 클릭 처리가 안전하게 소멸된 다음 프레임(0.02초 뒤)에 비동기로 뷰포트 포커스를 강제로 가져오고 마우스 캡처 및 게임모드를 완벽하게 복원합니다.
    if (UWorld* World = GetWorld()) {
      FTimerHandle TempHandle;
      TWeakObjectPtr<ANonPlayerController> WeakThis(this);
      World->GetTimerManager().SetTimer(TempHandle, [WeakThis]() {
        if (WeakThis.IsValid()) {
          WeakThis->SetShowMouseCursor(false);
          FInputModeGameOnly GameOnly;
          WeakThis->SetInputMode(GameOnly);
          WeakThis->bEnableClickEvents = false;
          WeakThis->bEnableMouseOverEvents = false;

          if (UWorld* InnerWorld = WeakThis->GetWorld()) {
            if (UGameViewportClient* ViewportClient = InnerWorld->GetGameViewport()) {
              ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
            }
            if (TSharedPtr<SViewport> VP = WeakThis->GetGameViewportSViewport(InnerWorld)) {
              const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
              FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedPtr<SWidget>(VP), EFocusCause::SetDirectly);
              FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
            }
          }
        }
      }, 0.02f, false);
    }
  }
}

void ANonPlayerController::OnToggleArmed(const FInputActionValue & /*Value*/) {
  if (!CachedChar)
    return;

  // 1) 무기 없으면: 무장 상태면 강제로 해제만 하고 끝
  if (UEquipmentComponent *Eq =
          CachedChar->FindComponentByClass<UEquipmentComponent>()) {
    if (!Eq->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain)) {
      if (CachedChar->IsArmed()) {
        CachedChar->SetArmed(false);
      }
      return;
    }
  }

  // 2) ASC 통해 GA_ToggleWeapon 발동
  UAbilitySystemComponent *ASC = nullptr;
  if (IAbilitySystemInterface *ASI =
          Cast<IAbilitySystemInterface>(CachedChar)) {
    ASC = ASI->GetAbilitySystemComponent();
  }
  if (!ASC)
    return;

  static const FGameplayTag ToggleTag =
      FGameplayTag::RequestGameplayTag(TEXT("Ability.ToggleWeapon"));

  FGameplayTagContainer TagContainer;
  TagContainer.AddTag(ToggleTag);

  const bool bSuccess = ASC->TryActivateAbilitiesByTag(TagContainer);
}

void ANonPlayerController::OnInventory() {
  if (APawn *P = GetPawn()) {
    if (UNonUIManagerComponent *UIMan =
            P->FindComponentByClass<UNonUIManagerComponent>()) {
      UIMan->ToggleInventory();
    } else {
    }
  }
}

void ANonPlayerController::OnToggleSkillWindow() {
  if (APawn *P = GetPawn()) {
    if (UNonUIManagerComponent *UIMan =
            P->FindComponentByClass<UNonUIManagerComponent>()) {
      UIMan->ToggleSkillWindow();
      return;
    }
  }
}

void ANonPlayerController::ToggleCursorLook() {
  if (bShowMouseCursor) {
    bCursorFree = false;
    SetShowMouseCursor(false);
    FInputModeGameOnly GameOnly;
    SetInputMode(GameOnly);
    bEnableClickEvents = false;
    bEnableMouseOverEvents = false;

    // [New] 마우스 커서 해제 단축키로 커서가 사라질 때 마우스 캡처 방식을 기본값으로 복구
    if (UWorld* World = GetWorld()) {
      if (UGameViewportClient* ViewportClient = World->GetGameViewport()) {
        ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
      }
    }
    return;
  }

  bCursorFree = true;
  SetShowMouseCursor(true);

  FInputModeGameAndUI GameAndUI;
  GameAndUI.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
  GameAndUI.SetHideCursorDuringCapture(false);
  GameAndUI.SetWidgetToFocus(nullptr);
  SetInputMode(GameAndUI);

  bEnableClickEvents = true;
  bEnableMouseOverEvents = true;

  // [New] 단축키를 눌러 마우스 커서가 나온 상태에서는 월드 클릭 시 커서가 사라지지 않도록 방지
  if (UWorld* World = GetWorld()) {
    if (UGameViewportClient* ViewportClient = World->GetGameViewport()) {
      ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
    }
  }
}



void ANonPlayerController::OnToggleCharacterWindow() {
  if (APawn *P = GetPawn()) {
    if (UNonUIManagerComponent *UIMan =
            P->FindComponentByClass<UNonUIManagerComponent>()) {
      UIMan->ToggleCharacter();
    } else {
    }
  }
}

void ANonPlayerController::OnGuardPressed(
    const FInputActionInstance & /*Instance*/) {
  if (!CachedChar)
    return;

  UAbilitySystemComponent *ASC = nullptr;
  if (IAbilitySystemInterface *ASI =
          Cast<IAbilitySystemInterface>(CachedChar)) {
    ASC = ASI->GetAbilitySystemComponent();
  }
  if (!ASC)
    return;

  static const FGameplayTag GuardTag =
      FGameplayTag::RequestGameplayTag(TEXT("Ability.Guard"));

  FGameplayTagContainer GuardTags;
  GuardTags.AddTag(GuardTag);

  ASC->TryActivateAbilitiesByTag(GuardTags);
}

void ANonPlayerController::OnGuardReleased(
    const FInputActionInstance & /*Instance*/) {
  if (!CachedChar)
    return;

  UAbilitySystemComponent *ASC = nullptr;
  if (IAbilitySystemInterface *ASI =
          Cast<IAbilitySystemInterface>(CachedChar)) {
    ASC = ASI->GetAbilitySystemComponent();
  }
  if (!ASC)
    return;

  static const FGameplayTag GuardTag =
      FGameplayTag::RequestGameplayTag(TEXT("Ability.Guard"));

  FGameplayTagContainer GuardTags;
  GuardTags.AddTag(GuardTag);

  ASC->CancelAbilities(&GuardTags);
}

void ANonPlayerController::HandleQuickSlot(int32 OneBased) {
  int32 ZeroBased = -1;
  if (OneBased == 10)
    ZeroBased = 9;
  else if (OneBased >= 1 && OneBased <= 9)
    ZeroBased = OneBased - 1;
  else {
    return;
  }

  if (!CachedQuick) {
    if (APawn *P = GetPawn()) {
      CachedQuick = P->FindComponentByClass<UQuickSlotManager>();
    }
  }

  if (CachedQuick) {
    CachedQuick->UseQuickSlot(ZeroBased);
  } else {
  }
}

void ANonPlayerController::OnDodge(const FInputActionValue &Value) {
  if (!CachedChar)
    return;

  FVector2D Input2D = FVector2D::ZeroVector;

  // 1) IA_Dodge 값
  if (Value.GetValueType() == EInputActionValueType::Axis2D) {
    Input2D = Value.Get<FVector2D>();
  }

  // 2) IA_Move 값
  if (Input2D.IsNearlyZero() && InputComponent) {
    if (const UEnhancedInputComponent *EIC =
            Cast<UEnhancedInputComponent>(InputComponent)) {
      if (IA_MoveCached) {
        const FInputActionValue MoveVal =
            EIC->GetBoundActionValue(IA_MoveCached);
        if (MoveVal.GetValueType() == EInputActionValueType::Axis2D) {
          Input2D = MoveVal.Get<FVector2D>();
        }
      }
    }
  }

  // 3) 최근 이동 입력
  if (Input2D.IsNearlyZero()) {
    const FVector Last = CachedChar->GetLastMovementInputVector();
    if (!Last.IsNearlyZero()) {
      const FRotator CtrlYaw(0.f, GetControlRotation().Yaw, 0.f);
      const FVector Fwd = FRotationMatrix(CtrlYaw).GetUnitAxis(EAxis::X);
      const FVector Right = FRotationMatrix(CtrlYaw).GetUnitAxis(EAxis::Y);

      const FVector L2D = Last.GetSafeNormal2D();
      Input2D.X = FVector::DotProduct(L2D, Fwd);
      Input2D.Y = FVector::DotProduct(L2D, Right);
    }
  }

  // 4) 속도 방향
  if (Input2D.IsNearlyZero()) {
    const FVector Vel = CachedChar->GetVelocity();
    if (Vel.SizeSquared2D() > KINDA_SMALL_NUMBER) {
      const FRotator CtrlYaw(0.f, GetControlRotation().Yaw, 0.f);
      const FVector Fwd = FRotationMatrix(CtrlYaw).GetUnitAxis(EAxis::X);
      const FVector Right = FRotationMatrix(CtrlYaw).GetUnitAxis(EAxis::Y);

      const FVector V2D = FVector(Vel.X, Vel.Y, 0.f).GetSafeNormal();
      Input2D.X = FVector::DotProduct(V2D, Fwd);
      Input2D.Y = FVector::DotProduct(V2D, Right);
    }
  }

  // 5) 정규화(대각/직각 유지)
  if (!Input2D.IsNearlyZero()) {
    const float ax = FMath::Abs(Input2D.X);
    const float ay = FMath::Abs(Input2D.Y);
    const float maxa = FMath::Max(ax, ay);
    if (maxa > SMALL_NUMBER) {
      Input2D /= maxa;
    }
  }

  // === 여기까지는 방향 계산용 (나중에 GA_Dodge에서 쓰고 싶으면 캐릭터에 저장)
  // ===

  // 지금은 GA_Dodge가 Char->GetLastMovementInputVector() 기준으로 방향
  // 계산하니까 Input2D는 그냥 무시하고, Ability만 실행해도 됨.
  CachedChar->TryDodge();
}

void ANonPlayerController::UpdateInteractFocus(float DeltaTime) {
  if (!IsLocalController())
    return;

  if (!CachedChar)
    return;

  // [New] 대화 카메라(시네마틱) 연출 중이거나, 상점이 열려 있거나, 종료 직후 쿨다운 중이면 상호작용 프롬프트 무시
  bool bIsShopOpen = false;
  if (UNonUIManagerComponent* UI = CachedChar->FindComponentByClass<UNonUIManagerComponent>()) {
      bIsShopOpen = UI->IsMerchantShopOpen();
  }

  if (CachedChar->bIsDialogueCameraActive || bIsShopOpen || DialogueEndCooldown > 0.f) {
      if (UNonUIManagerComponent *UI = CachedChar->FindComponentByClass<UNonUIManagerComponent>()) {
          UI->HideInteractPrompt();
      }
      
      // 혹시 하이라이트된 타겟이 남아있다면 끔
      if (AActor* OldTarget = CurrentInteractTarget.Get()) {
          if (OldTarget->GetClass()->ImplementsInterface(UNonInteractableInterface::StaticClass())) {
              INonInteractableInterface::Execute_SetInteractHighlight(OldTarget, false);
          }
          CurrentInteractTarget = nullptr;
      }
      return;
  }

  // ── [상호작용 메뉴가 열려 있는 경우] ──
  if (PlayerInteractionMenuWidget && PlayerInteractionMenuWidget->IsInViewport() &&
      PlayerInteractionMenuWidget->GetVisibility() == ESlateVisibility::Visible)
  {
      // 1) 대상 플레이어와 거리가 멀어졌다면 메뉴 닫기
      ANonCharacterBase* TargetChar = PlayerInteractionMenuWidget->GetTargetCharacter();
      if (!TargetChar || FVector::Dist2D(CachedChar->GetActorLocation(), TargetChar->GetActorLocation()) > 180.f)
      {
          PlayerInteractionMenuWidget->CloseMenu();
          return;
      }

      // 2) 메뉴가 열려 있는 동안에는 [F] 상호작용 프롬프트 팝업 숨기기 및 추가 탐색 중단
      if (UNonUIManagerComponent *UI = CachedChar->FindComponentByClass<UNonUIManagerComponent>())
      {
          UI->HideInteractPrompt();
      }
      return;
  }

  // 캐릭터 캡슐 정보 가져오기
  UCapsuleComponent *Capsule = CachedChar->GetCapsuleComponent();
  if (!Capsule)
    return;

  const float TraceDistance = 110.f; // 상호작용 적정 거리 (약 1.1미터, 근접 대화 거리)

  // 캡슐 중심 기준에서 앞쪽으로 스윕 (카메라 시선 방향 및 캐릭터 전방 방향 탐색)
  FVector Start = CachedChar->GetActorLocation();
  FVector ForwardDir = GetControlRotation().Vector();
  ForwardDir.Z = FMath::Clamp(ForwardDir.Z, -0.6f, 0.6f);
  ForwardDir.Normalize();
  FVector End = Start + ForwardDir * TraceDistance;

  // 조준을 더 정확하게 감지하도록 콤팩트한 스피어(반경 15cm)로 스윕
  FCollisionShape Shape = FCollisionShape::MakeSphere(15.f);

  FHitResult Hit;
  FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractFocus), false,
                               CachedChar);

  bool bHit = GetWorld()->SweepSingleByChannel(
      Hit, Start, End, FQuat::Identity, InteractChannel, Shape, Params);

  // 카메라 시선 방향에서 대상을 찾지 못했으면 캐릭터 몸통 전방으로 2차 탐색
  if (!bHit || !Hit.GetActor() || !Hit.GetActor()->Implements<UNonInteractableInterface>() || Hit.GetActor() == CachedChar) {
    End = Start + CachedChar->GetActorForwardVector() * TraceDistance;
    bHit = GetWorld()->SweepSingleByChannel(
        Hit, Start, End, FQuat::Identity, InteractChannel, Shape, Params);
  }

  // 💡 [핵심] 캐릭터 간 실제 물리적 수평 거리(Dist2D)를 직접 검사하여 멀리서 뜨는 현상 원천 차단!
  // 캡슐 반지름(42cm + 42cm = 84cm) + 여유 간격(약 66cm) = 최대 150cm (1.5m 이내 초근접 시에만 상호작용 활성화)
  constexpr float MaxInteractPhysicalDistance = 150.f;

  AActor *NewTarget = nullptr;
  if (bHit && Hit.GetActor() && Hit.GetActor() != CachedChar &&
      Hit.GetActor()->Implements<UNonInteractableInterface>()) {
    FVector TargetPos = Hit.GetComponent() ? Hit.GetComponent()->GetComponentLocation() : Hit.GetActor()->GetActorLocation();
    const float ActualDist = FVector::Dist2D(CachedChar->GetActorLocation(), TargetPos);
    if (ActualDist <= MaxInteractPhysicalDistance)
    {
      NewTarget = Hit.GetActor();
    }
  }

  UNonUIManagerComponent *UI =
      CachedChar->FindComponentByClass<UNonUIManagerComponent>();

  // 1) 아무 것도 안 맞았으면 → 프롬프트 끄기 + 하이라이트 해제
  if (!NewTarget) {
    AActor *OldTarget = CurrentInteractTarget.Get();
    CurrentInteractTarget = nullptr;

    if (OldTarget && OldTarget->GetClass()->ImplementsInterface(
                         UNonInteractableInterface::StaticClass())) {
      INonInteractableInterface::Execute_SetInteractHighlight(OldTarget, false);
    }

    if (UI) {
      UI->HideInteractPrompt();
    }
    return;
  }

  // 2) 새 타겟으로 변경됐을 때만 처리
  if (NewTarget != CurrentInteractTarget.Get()) {
    AActor *OldTarget = CurrentInteractTarget.Get();
    CurrentInteractTarget = NewTarget;

    // 이전 타겟 하이라이트 해제
    if (OldTarget && OldTarget->GetClass()->ImplementsInterface(
                         UNonInteractableInterface::StaticClass())) {
      INonInteractableInterface::Execute_SetInteractHighlight(OldTarget, false);
    }

    // 새 타겟 하이라이트 on
    if (NewTarget->GetClass()->ImplementsInterface(
            UNonInteractableInterface::StaticClass())) {
      INonInteractableInterface::Execute_SetInteractHighlight(NewTarget, true);
    }

    if (UI) {
      FText Label = FText::FromString(TEXT("상호작용"));

      if (NewTarget->GetClass()->ImplementsInterface(
              UNonInteractableInterface::StaticClass())) {
        Label = INonInteractableInterface::Execute_GetInteractLabel(NewTarget);
      }

      UI->ShowInteractPrompt(Label);
    }
  }
}

void ANonPlayerController::OnEsc(const FInputActionInstance & /*Instance*/) {
  // 1) UI 매니저에게 창 닫기 요청
  if (APawn *P = GetPawn()) {
    if (UNonUIManagerComponent *UIMan =
            P->FindComponentByClass<UNonUIManagerComponent>()) {
      if (UIMan->CloseTopWindow()) {
        // UI가 하나라도 닫혔으면 여기서 끝 (메뉴 안 띄움)
        return;
      }
    }
  }

  // 2) 닫을 UI가 없었다면 -> 게임 메뉴(시스템 메뉴) 띄우기
  if (APawn *P = GetPawn()) {
    if (UNonUIManagerComponent *UIMan =
            P->FindComponentByClass<UNonUIManagerComponent>()) {
      UIMan->ToggleWindow(EGameWindowType::SystemMenu);
    }
  }
}

void ANonPlayerController::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty> &OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);
  DOREPLIFETIME(ANonPlayerController, SelectedSlotIndex);
  DOREPLIFETIME(ANonPlayerController, CurrentDuelOpponent);
  DOREPLIFETIME(ANonPlayerController, PlayerNickname);
}

void ANonPlayerController::StartDialogueCooldown(float Duration) {
  DialogueEndCooldown = Duration;
}

void ANonPlayerController::ShowTitleUI() {
  if (!IsLocalController())
    return;

  // 만약 타이틀 위젯이 지정되지 않았다면 바로 캐릭터 선택으로 이동 (Fallback)
  if (!TitleWidgetClass) {
    GoToCharacterSelect();
    return;
  }

  if (CurrentWidget) {
    CurrentWidget->RemoveFromParent();
    CurrentWidget = nullptr;
  }

  if (UUserWidget *Widget = CreateWidget<UUserWidget>(this, TitleWidgetClass))
  {
    Widget->AddToViewport();
    CurrentWidget = Widget;

    // 입력 모드 UI Only
    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    bShowMouseCursor = true;
  }
}

void ANonPlayerController::GoToCharacterSelect() { ShowCharacterSelectUI(); }

void ANonPlayerController::ShowCharacterSelectUI() {
  if (!IsLocalController())
    return;

  if (!CharacterSelectWidgetClass)
  {
    return;
  }

  if (CurrentWidget) {
    CurrentWidget->RemoveFromParent();
    CurrentWidget = nullptr;
  }

  if (UUserWidget *Widget =
          CreateWidget<UUserWidget>(this, CharacterSelectWidgetClass))
  {
    Widget->AddToViewport();
    CurrentWidget = Widget;

    // 입력 모드 UI Only
    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    bShowMouseCursor = true;
  }
}

void ANonPlayerController::StartCharacterCreation(int32 SlotIndex) {
  if (!IsLocalController())
    return;

  if (!CharacterCreationWidgetClass)
  {
    return;
  }

  if (CurrentWidget) {
    CurrentWidget->RemoveFromParent();
    CurrentWidget = nullptr;
  }

  if (UCharacterCreationWidget *Widget = CreateWidget<UCharacterCreationWidget>(
          this, CharacterCreationWidgetClass))
  {

    Widget->TargetSlotIndex = SlotIndex; // 슬롯 정보 전달
    Widget->AddToViewport();
    CurrentWidget = Widget;

    // 입력 모드 UI Only
    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    bShowMouseCursor = true;
  }
}

void ANonPlayerController::OnCharacterCreationFinished() {
  ShowCharacterSelectUI();
}

void ANonPlayerController::ServerSpawnCharacter_Implementation(
    int32 SlotIndex) {
  SelectedSlotIndex = SlotIndex;

  if (UWorld *World = GetWorld()) {
    if (ANonGameMode *GM = Cast<ANonGameMode>(World->GetAuthGameMode())) {
      GM->SpawnPlayerFromSave(this, SlotIndex);

      // 스폰 완료 알림
      ClientOnSpawnFinished();
    } else {
    }
  }
}

void ANonPlayerController::ClientOnSpawnFinished_Implementation() {
  // UI 제거
  if (CurrentWidget) {
    CurrentWidget->RemoveFromParent();
    CurrentWidget = nullptr;
  }

  // 입력 모드 게임으로 복구
  FInputModeGameOnly GameMode;
  SetInputMode(GameMode);
  bShowMouseCursor = false;
  bEnableClickEvents = false;
  bEnableMouseOverEvents = false;
}

void ANonPlayerController::ServerStartGame_Implementation(
    const FString &MapName) {
  if (UWorld *World = GetWorld()) {
    // 서버 측에서 맵 이동 (Seamless Travel 권장)
    // World->ServerTravel(MapName + TEXT("?listen")); // 필요하다면 옵션 추가
    World->ServerTravel(MapName);
  }
}

void ANonPlayerController::ShowGameOverUI_Implementation() {
  if (!GameOverWidgetClass) return;

  if (CurrentWidget) {
    CurrentWidget->RemoveFromParent();
    CurrentWidget = nullptr;
  }

  if (UUserWidget* Widget = CreateWidget<UUserWidget>(this, GameOverWidgetClass)) {
    // 가장 위에 보이게 ZOrder 9999
    Widget->AddToViewport(9999);
    CurrentWidget = Widget;

    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
  }
}

void ANonPlayerController::ServerRespawnPlayer_Implementation(bool bInPlace) {
  if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetPawn())) {
    bool bHasAmulet = false;
    int32 AmuletSlotIndex = INDEX_NONE;

    UInventoryComponent* InvComp = Char->GetInventoryComponent();
    if (InvComp) {
      for (int32 i = 0; i < InvComp->Slots.Num(); ++i) {
        if (UInventoryItem* Item = InvComp->Slots[i]) {
          if (Item->ItemId == Char->ResurrectionAmuletItemId && Item->Quantity > 0) {
            bHasAmulet = true;
            AmuletSlotIndex = i;
            break;
          }
        }
      }
    }

    if (bInPlace && bHasAmulet && InvComp) {
      // 1. 부적 사용 제자리 부활 (부적 소모, 패널티 없음)
      InvComp->RemoveAt(AmuletSlotIndex, 1);
      Char->Revive(true);
    } else {
      // 2. 기본 부활 (가장 가까운 부활석, 경험치 하락 및 디버프 적용)
      AActor* BestStart = nullptr;
      float MinDistSq = MAX_flt;
      
      for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) {
         float DistSq = It->GetSquaredDistanceTo(Char);
         if (DistSq < MinDistSq) {
             MinDistSq = DistSq;
             BestStart = *It;
         }
      }
      
      if (BestStart) {
         Char->SetActorLocation(BestStart->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
         Char->SetActorRotation(BestStart->GetActorRotation());
      }

      Char->ApplyResurrectionPenalty();
      Char->Revive(false);
    }
    
    // 사망 UI 닫기 및 입력 모드 복귀
    ClientOnSpawnFinished(); 
  }
}

bool ANonPlayerController::Server_RequestDuel_Validate(ANonPlayerController* TargetPlayer) {
  return true;
}

void ANonPlayerController::Server_RequestDuel_Implementation(ANonPlayerController* TargetPlayer) {
  if (!TargetPlayer || TargetPlayer == this || CurrentDuelOpponent || TargetPlayer->CurrentDuelOpponent) {
    return;
  }
  TargetPlayer->Client_ReceiveDuelRequest(this);
  Client_AddSystemMessage(FString::Printf(TEXT("%s 님에게 1:1 결투를 신청했습니다!"), *TargetPlayer->GetPlayerNickname()));
}

bool ANonPlayerController::Server_RequestDuelByCharacter_Validate(ANonCharacterBase* TargetChar) {
  return true;
}

void ANonPlayerController::Server_RequestDuelByCharacter_Implementation(ANonCharacterBase* TargetChar) {
  if (!TargetChar) return;

  ANonPlayerController* TargetPC = Cast<ANonPlayerController>(TargetChar->GetController());
  if (!TargetPC && GetWorld()) {
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) {
      if (ANonPlayerController* NonPC = Cast<ANonPlayerController>(It->Get())) {
        if (NonPC->GetPawn() == TargetChar || NonPC->GetCharacter() == TargetChar) {
          TargetPC = NonPC;
          break;
        }
      }
    }
  }

  if (TargetPC) {
    Server_RequestDuel(TargetPC);
  }
}

void ANonPlayerController::Client_ReceiveDuelRequest_Implementation(ANonPlayerController* Requester) {
  if (!Requester) return;

  FString ReqName = Requester->GetPlayerNickname();
  Client_AddSystemMessage(FString::Printf(TEXT("%s 님이 1:1 결투를 신청했습니다! [수락: Y / 거절: N]"), *ReqName));
  ShowInteractionRequest(EInteractionRequestType::DuelRequest, ReqName, Requester, 15.f);
}

bool ANonPlayerController::Server_AcceptDuel_Validate(ANonPlayerController* Requester) {
  return true;
}

void ANonPlayerController::Server_AcceptDuel_Implementation(ANonPlayerController* Requester) {
  if (!Requester || CurrentDuelOpponent || Requester->CurrentDuelOpponent) {
    return;
  }
  
  ANonCharacterBase* MyChar = Cast<ANonCharacterBase>(GetPawn());
  ANonCharacterBase* ReqChar = Cast<ANonCharacterBase>(Requester->GetPawn());
  
  if (!MyChar || !ReqChar || MyChar->IsDead() || ReqChar->IsDead()) {
    return;
  }
  
  float Dist = FVector::Dist(MyChar->GetActorLocation(), ReqChar->GetActorLocation());
  if (Dist > DuelMaxDistance) {
    return; 
  }
  
  CurrentDuelOpponent = Requester;
  Requester->CurrentDuelOpponent = this;
  
  FGameplayTag DuelTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combat.Dueling"));
  if (MyChar->GetAbilitySystemComponent()) {
    MyChar->GetAbilitySystemComponent()->AddLooseGameplayTag(DuelTag);
  }
  if (ReqChar->GetAbilitySystemComponent()) {
    ReqChar->GetAbilitySystemComponent()->AddLooseGameplayTag(DuelTag);
  }
  
  GetWorldTimerManager().SetTimer(DuelDistanceCheckTimerHandle, this, &ANonPlayerController::CheckDuelDistanceAndRules, 0.5f, true);
  
  Client_AddSystemMessage(FString::Printf(TEXT("%s 님과의 1:1 결투가 시작되었습니다!"), *Requester->GetPlayerNickname()));
  Requester->Client_AddSystemMessage(FString::Printf(TEXT("%s 님과의 1:1 결투가 시작되었습니다!"), *GetPlayerNickname()));

  if (GEngine) {
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("결투가 시작되었습니다!"));
  }
}

bool ANonPlayerController::Server_DeclineDuel_Validate(ANonPlayerController* Requester) {
  return true;
}

void ANonPlayerController::Server_DeclineDuel_Implementation(ANonPlayerController* Requester) {
  if (!Requester) return;
  
  ANonCharacterBase* MyChar = Cast<ANonCharacterBase>(GetPawn());
  FString MyName = MyChar ? MyChar->GetPlayerName() : GetName();
  
  Client_AddSystemMessage(FString::Printf(TEXT("%s 님의 결투 신청을 거절했습니다."), *Requester->GetPlayerNickname()));
  Requester->Client_DeclineDuelNotification(MyName);
}

void ANonPlayerController::Client_DeclineDuelNotification_Implementation(const FString& RefuserName) {
  Client_AddSystemMessage(FString::Printf(TEXT("%s 님이 결투 신청을 거절했습니다."), *RefuserName));
  if (GEngine) {
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, FString::Printf(TEXT("%s 님이 결투 신청을 거절했습니다."), *RefuserName));
  }
}

void ANonPlayerController::Multicast_EndDuel_Implementation(ANonPlayerController* Winner, ANonPlayerController* Loser, bool bDraw) {
  ANonCharacterBase* MyChar = Cast<ANonCharacterBase>(GetPawn());
  if (MyChar && MyChar->GetAbilitySystemComponent()) {
    FGameplayTag DuelTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combat.Dueling"));
    MyChar->GetAbilitySystemComponent()->RemoveLooseGameplayTag(DuelTag);
  }
  
  CurrentDuelOpponent = nullptr;
  GetWorldTimerManager().ClearTimer(DuelDistanceCheckTimerHandle);
  
  if (GEngine) {
    if (bDraw) {
      GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("결투 시간 초과로 무승부 처리되었습니다."));
    } else {
      if (Winner == this) {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("결투에서 승리했습니다!"));
      } else if (Loser == this) {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("결투에서 패배했습니다."));
      }
    }
  }
}

void ANonPlayerController::CheckDuelDistanceAndRules() {
  if (!HasAuthority()) return;
  
  if (!CurrentDuelOpponent) {
    GetWorldTimerManager().ClearTimer(DuelDistanceCheckTimerHandle);
    return;
  }
  
  ANonCharacterBase* MyChar = Cast<ANonCharacterBase>(GetPawn());
  ANonCharacterBase* OppChar = Cast<ANonCharacterBase>(CurrentDuelOpponent->GetPawn());
  
  if (!MyChar || !OppChar) {
    Multicast_EndDuel(nullptr, nullptr, true);
    return;
  }
  
  float Dist = FVector::Dist(MyChar->GetActorLocation(), OppChar->GetActorLocation());
  if (Dist > DuelMaxDistance) {
    Multicast_EndDuel(CurrentDuelOpponent, this);
  }
}

void ANonPlayerController::OnToggleChat()
{
  if (!IsLocalController()) return;

  if (APawn* MyPawn = GetPawn())
  {
    if (UNonUIManagerComponent* UIMgr = MyPawn->FindComponentByClass<UNonUIManagerComponent>())
    {
      if (UInGameHUD* HUD = UIMgr->GetInGameHUD())
      {
        if (HUD->IsChatInputFocused())
        {
          HUD->UnfocusChatInput();
        }
        else
        {
          HUD->FocusChatInput();
        }
      }
    }
  }
}

void ANonPlayerController::Server_SendChatMessage_Implementation(const FChatMessage& Message)
{
  if (!HasAuthority() || !GetWorld()) return;

  // 1. 파티 채팅 (Party) 분기: 오직 동일한 파티원들에게만 전송
  if (Message.Channel == EChatChannel::Party)
  {
    UPartyComponent* SenderPartyComp = GetPartyComponent();
    if (!SenderPartyComp || !SenderPartyComp->IsInParty())
    {
      Client_AddSystemMessage(TEXT("파티에 가입되어 있지 않습니다."));
      return;
    }

    const FGuid SenderPartyId = SenderPartyComp->GetPartyData().PartyId;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
      if (ANonPlayerController* TargetPC = Cast<ANonPlayerController>(It->Get()))
      {
        if (UPartyComponent* TargetPartyComp = TargetPC->GetPartyComponent())
        {
          if (TargetPartyComp->IsInParty() && TargetPartyComp->GetPartyData().PartyId == SenderPartyId)
          {
            TargetPC->Client_ReceiveChatMessage(Message);
          }
        }
      }
    }
    return;
  }

  // 2. 일반 채팅 등 기타 브로드캐스트
  for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
  {
    if (ANonPlayerController* PC = Cast<ANonPlayerController>(It->Get()))
    {
      PC->Client_ReceiveChatMessage(Message);
    }
  }
}

void ANonPlayerController::Client_ReceiveChatMessage_Implementation(const FChatMessage& Message)
{
  if (!IsLocalController()) return;

  bool bDelivered = false;
  if (APawn* MyPawn = GetPawn())
  {
    if (UNonUIManagerComponent* UIMgr = MyPawn->FindComponentByClass<UNonUIManagerComponent>())
    {
      if (UInGameHUD* HUD = UIMgr->GetInGameHUD())
      {
        HUD->AddChatMessage(Message);
        bDelivered = true;
      }
    }
  }

  // Fallback: Pawn이나 UIManager에서 못 찾은 경우 월드 뷰포트의 InGameHUD 검색
  if (!bDelivered && GetWorld())
  {
    TArray<UUserWidget*> FoundHUDs;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundHUDs, UInGameHUD::StaticClass(), false);
    for (UUserWidget* Widget : FoundHUDs)
    {
      if (UInGameHUD* HUD = Cast<UInGameHUD>(Widget))
      {
        HUD->AddChatMessage(Message);
        bDelivered = true;
        break;
      }
    }
  }
}

void ANonPlayerController::Client_AddSystemMessage_Implementation(const FString& MessageText)
{
  if (!IsLocalController()) return;

  // 1. [시스템] 접두사 중복 방지 (이미 들어있다면 깨끗하게 제거 후 전달)
  FString CleanText = MessageText;
  if (CleanText.StartsWith(TEXT("[시스템]")))
  {
    CleanText = CleanText.Mid(5).TrimStart();
  }
  else if (CleanText.StartsWith(TEXT("[System]"), ESearchCase::IgnoreCase))
  {
    CleanText = CleanText.Mid(8).TrimStart();
  }

  FChatMessage SysMsg(EChatChannel::System, TEXT("시스템"), CleanText);
  SysMsg.CustomColor = FLinearColor(1.0f, 0.85f, 0.2f); // 따뜻한 시스템 메시지 황금색

  bool bDelivered = false;

  // 2. Pawn의 UIManager를 통한 HUD 전달 시도
  if (APawn* MyPawn = GetPawn())
  {
    if (UNonUIManagerComponent* UIMgr = MyPawn->FindComponentByClass<UNonUIManagerComponent>())
    {
      if (UInGameHUD* HUD = UIMgr->GetInGameHUD())
      {
        HUD->AddChatMessage(SysMsg);
        bDelivered = true;
      }
    }
  }

  // 3. Fallback: Pawn 또는 UIManager에서 HUD를 못 찾았을 경우 뷰포트 위젯 직접 탐색
  if (!bDelivered && GetWorld())
  {
    TArray<UUserWidget*> FoundHUDs;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundHUDs, UInGameHUD::StaticClass(), false);
    for (UUserWidget* Widget : FoundHUDs)
    {
      if (UInGameHUD* HUD = Cast<UInGameHUD>(Widget))
      {
        HUD->AddChatMessage(SysMsg);
        bDelivered = true;
        break;
      }
    }

    if (!bDelivered)
    {
      TArray<UUserWidget*> FoundChatBoxes;
      UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundChatBoxes, UChatBoxWidget::StaticClass(), false);
      for (UUserWidget* Widget : FoundChatBoxes)
      {
        if (UChatBoxWidget* ChatBox = Cast<UChatBoxWidget>(Widget))
        {
          ChatBox->AddChatMessage(SysMsg);
          bDelivered = true;
          break;
        }
      }
    }
  }

  // 4. 화면 상단 실시간 디버그 피드백 (HUD 미표시 상황에서도 무조건 보장)
  if (GEngine)
  {
    GEngine->AddOnScreenDebugMessage(-1, 4.5f, FColor(255, 215, 0), FString::Printf(TEXT("[시스템] %s"), *CleanText));
  }
}

FString ANonPlayerController::GetPlayerNickname() const
{
  // 1순위: 폰(캐릭터)이 있고 캐릭터 이름이 유효하다면 최우선 반환 (머리 위 이름과 100% 일치 보장)
  if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetPawn()))
  {
    const FString CharName = Char->GetPlayerName().TrimStartAndEnd();
    if (!CharName.IsEmpty() && !CharName.Equals(TEXT("Player")))
    {
      return CharName;
    }
  }

  // 2순위: 컨트롤러에 저장된 닉네임이 있다면 반환
  if (!PlayerNickname.IsEmpty())
  {
    return PlayerNickname.TrimStartAndEnd();
  }

  // 3순위: 개발/테스트용 기본 임의 닉네임
  const int32 UniqueNum = static_cast<int32>(GetUniqueID() % 100) + 1;
  return FString::Printf(TEXT("버서커_%d"), UniqueNum);
}

void ANonPlayerController::SetPlayerNickname(const FString& NewNickname)
{
  PlayerNickname = NewNickname;
}

void ANonPlayerController::ShowPlayerInteractionMenu(ANonCharacterBase* TargetCharacter)
{
  if (!TargetCharacter || !IsLocalController()) return;

  // 💡 상호작용 메뉴를 열 때 기존 [F] 상호작용 프롬프트 팝업 즉시 숨기기
  if (CachedChar)
  {
    if (UNonUIManagerComponent* UI = CachedChar->FindComponentByClass<UNonUIManagerComponent>())
    {
      UI->HideInteractPrompt();
    }
  }

  FString TargetName = TargetCharacter->GetPlayerName();
  if (TargetName.IsEmpty())
  {
    TargetName = TEXT("플레이어");
  }

  // 상호작용 목록 UI 위젯 생성 및 초기화 (미지정 시 기본 WBP 자동 로드)
  if (!PlayerInteractionMenuWidgetClass)
  {
    PlayerInteractionMenuWidgetClass = LoadClass<UUserWidget>(
        nullptr, TEXT("/Game/Non/UI/WBP_PlayerInteractionMenu.WBP_PlayerInteractionMenu_C"));
  }

  if (!PlayerInteractionMenuWidget && PlayerInteractionMenuWidgetClass)
  {
    PlayerInteractionMenuWidget = CreateWidget<UPlayerInteractionMenuWidget>(this, PlayerInteractionMenuWidgetClass);
  }

  if (PlayerInteractionMenuWidget)
  {
    PlayerInteractionMenuWidget->InitializeMenu(TargetCharacter);
    if (!PlayerInteractionMenuWidget->IsInViewport())
    {
      PlayerInteractionMenuWidget->AddToViewport(150);
    }
    PlayerInteractionMenuWidget->SetVisibility(ESlateVisibility::Visible);
  }
  else if (PlayerInteractionMenuWidgetClass)
  {
    if (UUserWidget* GenericWidget = CreateWidget<UUserWidget>(this, PlayerInteractionMenuWidgetClass))
    {
      if (!GenericWidget->IsInViewport())
      {
        GenericWidget->AddToViewport(150);
      }
      GenericWidget->SetVisibility(ESlateVisibility::Visible);
    }
  }

  // 마우스 커서 표시 및 UI 상호작용 가능 모드로 전환
  bShowMouseCursor = true;
  FInputModeGameAndUI Mode;
  Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
  Mode.SetHideCursorDuringCapture(false);
  if (PlayerInteractionMenuWidget)
  {
    Mode.SetWidgetToFocus(PlayerInteractionMenuWidget->TakeWidget());
  }
  SetInputMode(Mode);

  // 블루프린트 위젯(WBP_PlayerInteractionMenu) 이벤트도 호출
  OnShowPlayerInteractionMenu(TargetCharacter, TargetName);
}

void ANonPlayerController::ShowInteractionRequest(EInteractionRequestType RequestType, const FString& RequesterName, ANonPlayerController* RequesterPC, float TimeoutSeconds)
{
  if (!IsLocalController())
  {
    return;
  }

  FString TypeName = TEXT("상호작용");
  switch (RequestType)
  {
  case EInteractionRequestType::PartyInvite:  TypeName = TEXT("파티 초대"); break;
  case EInteractionRequestType::TradeRequest: TypeName = TEXT("1:1 거래 신청"); break;
  case EInteractionRequestType::DuelRequest:  TypeName = TEXT("1:1 결투 신청"); break;
  }

  if (GEngine)
  {
    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Green,
        FString::Printf(TEXT("[요청 수신] %s 님이 %s를 보냈습니다! (수락: Y / 거절: N)"), *RequesterName, *TypeName));
  }

  if (!InteractionRequestWidgetClass)
  {
    InteractionRequestWidgetClass = LoadClass<UUserWidget>(
        nullptr, TEXT("/Game/Non/UI/WBP_InteractionRequestPopup.WBP_InteractionRequestPopup_C"));
    if (!InteractionRequestWidgetClass)
    {
      InteractionRequestWidgetClass = StaticLoadClass(
          UUserWidget::StaticClass(), nullptr, TEXT("/Game/Non/UI/WBP_InteractionRequestPopup.WBP_InteractionRequestPopup_C"));
    }
  }

  if (!InteractionRequestWidgetClass)
  {
    if (GEngine)
    {
      GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red,
          TEXT("[UI 안내] Content/Non/UI/WBP_InteractionRequestPopup 위젯 블루프린트 로드에 실패했습니다."));
    }
  }
  else
  {
    if (!InteractionRequestWidget || !InteractionRequestWidget->IsInViewport())
    {
      InteractionRequestWidget = CreateWidget<UInteractionRequestWidget>(this, InteractionRequestWidgetClass);
    }

    if (InteractionRequestWidget)
    {
      InteractionRequestWidget->SetupRequest(RequestType, RequesterName, RequesterPC, TimeoutSeconds);
      if (!InteractionRequestWidget->IsInViewport())
      {
        InteractionRequestWidget->AddToViewport(200);
      }
      InteractionRequestWidget->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
      // 만약 캐스팅 생성에 실패했다면 일반 위젯으로라도 띄움
      if (UUserWidget* FallbackWidget = CreateWidget<UUserWidget>(this, InteractionRequestWidgetClass))
      {
        if (!FallbackWidget->IsInViewport())
        {
          FallbackWidget->AddToViewport(200);
        }
        FallbackWidget->SetVisibility(ESlateVisibility::Visible);
      }
    }
  }

  OnShowInteractionRequest(RequestType, RequesterName);
}

// ── [직통 RPC] 파티 초대 및 거래 신청 구현 ──

bool ANonPlayerController::Server_SendPartyInviteDirect_Validate(ANonCharacterBase* TargetChar) {
  return true;
}

void ANonPlayerController::Server_SendPartyInviteDirect_Implementation(ANonCharacterBase* TargetChar) {
  if (GEngine) {
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Magenta,
        FString::Printf(TEXT("[서버 Direct수신] TargetChar: %s"), TargetChar ? *TargetChar->GetName() : TEXT("NULL")));
  }

  if (!TargetChar) return;

  ANonPlayerController* TargetPC = Cast<ANonPlayerController>(TargetChar->GetController());
  if (!TargetPC && GetWorld()) {
    const FString TargetCharName = TargetChar->GetPlayerName().TrimStartAndEnd();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) {
      if (ANonPlayerController* NonPC = Cast<ANonPlayerController>(It->Get())) {
        if (NonPC->GetPawn() == TargetChar || NonPC->GetCharacter() == TargetChar) {
          TargetPC = NonPC;
          break;
        }

        const FString PCNick = NonPC->GetPlayerNickname().TrimStartAndEnd();
        FString PawnCharName = TEXT("");
        if (ANonCharacterBase* PawnChar = Cast<ANonCharacterBase>(NonPC->GetPawn())) {
          PawnCharName = PawnChar->GetPlayerName().TrimStartAndEnd();
        }

        if (!TargetCharName.IsEmpty() && (PCNick.Equals(TargetCharName, ESearchCase::IgnoreCase) || PawnCharName.Equals(TargetCharName, ESearchCase::IgnoreCase))) {
          TargetPC = NonPC;
          break;
        }
      }
    }
  }

  if (!TargetPC || TargetPC == this) {
    if (TargetPC == this) {
      Client_AddSystemMessage(TEXT("[시스템] 자기 자신은 파티에 초대할 수 없습니다."));
    }
    return;
  }

  // 파티 초대 사전 유효성 검사 (이미 같은 파티, 다른 파티, 권한, 정원 초과 등)
  UPartyComponent* PartyComp = GetPartyComponent();
  if (!PartyComp) {
    PartyComp = FindComponentByClass<UPartyComponent>();
  }

  if (PartyComp) {
    FString FailReason;
    if (!PartyComp->CanInvite(TargetPC, FailReason)) {
      Client_AddSystemMessage(FailReason);
      return;
    }
    PartyComp->InvitePartyInternal(TargetPC);
  }

  if (GEngine) {
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
        FString::Printf(TEXT("[서버] Direct 파티 초대 전달: %s -> %s"), *GetPlayerNickname(), *TargetPC->GetPlayerNickname()));
  }

  // 1. 상대방 컨트롤러에 초대 알림 직통 전송 (RPC 전달 100% 보장)
  TargetPC->PendingPartyInviterPC = this;
  TargetPC->Client_ReceivePartyInviteDirect(GetPlayerNickname(), this);

  // 2. 초대를 보낸 나 자신에게 시스템 메시지 전송
  Client_AddSystemMessage(FString::Printf(TEXT("[시스템] %s 님에게 파티 초대를 보냈습니다."), *TargetPC->GetPlayerNickname()));
}

void ANonPlayerController::Client_ReceivePartyInviteDirect_Implementation(const FString& InviterNickname, ANonPlayerController* InviterPC) {
  PendingPartyInviterPC = InviterPC;

  if (GEngine) {
    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Orange,
        FString::Printf(TEXT("[클라 수신] %s 님의 파티 초대 도착!"), *InviterNickname));
  }

  Client_AddSystemMessage(FString::Printf(TEXT("%s 님이 파티에 초대했습니다. [수락: Y / 거절: N]"), *InviterNickname));
  ShowInteractionRequest(EInteractionRequestType::PartyInvite, InviterNickname, InviterPC, 15.f);
}

bool ANonPlayerController::Server_RespondPartyInviteDirect_Validate(bool bAccept) {
  return true;
}

void ANonPlayerController::Server_RespondPartyInviteDirect_Implementation(bool bAccept) {
  if (PendingPartyInviterPC.IsValid()) {
    if (bAccept) {
      Client_AddSystemMessage(FString::Printf(TEXT("%s 님의 파티 초대를 수락했습니다."), *PendingPartyInviterPC->GetPlayerNickname()));
      PendingPartyInviterPC->Client_AddSystemMessage(FString::Printf(TEXT("%s 님이 파티 초대를 수락했습니다."), *GetPlayerNickname()));
    } else {
      Client_AddSystemMessage(FString::Printf(TEXT("%s 님의 파티 초대를 거절했습니다."), *PendingPartyInviterPC->GetPlayerNickname()));
      PendingPartyInviterPC->Client_AddSystemMessage(FString::Printf(TEXT("%s 님이 파티 초대를 거절했습니다."), *GetPlayerNickname()));
    }

    if (UPartyComponent* MyParty = GetPartyComponent()) {
      MyParty->SetPendingInviterPC(PendingPartyInviterPC.Get());
      MyParty->Server_RespondPartyInvite(bAccept);
    }
  }
  PendingPartyInviterPC = nullptr;
}

bool ANonPlayerController::Server_SendTradeRequestDirect_Validate(ANonCharacterBase* TargetChar) {
  return true;
}

void ANonPlayerController::Server_SendTradeRequestDirect_Implementation(ANonCharacterBase* TargetChar) {
  if (!TargetChar) return;

  ANonPlayerController* TargetPC = Cast<ANonPlayerController>(TargetChar->GetController());
  if (!TargetPC && GetWorld()) {
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) {
      if (ANonPlayerController* NonPC = Cast<ANonPlayerController>(It->Get())) {
        if (NonPC->GetPawn() == TargetChar || NonPC->GetCharacter() == TargetChar) {
          TargetPC = NonPC;
          break;
        }
      }
    }
  }

  if (!TargetPC || TargetPC == this) return;

  TargetPC->PendingTradeRequesterPC = this;
  TargetPC->Client_ReceiveTradeRequestDirect(GetPlayerNickname(), this);

  Client_AddSystemMessage(FString::Printf(TEXT("%s 님에게 1:1 개인 거래를 신청했습니다."), *TargetPC->GetPlayerNickname()));

  if (GEngine) {
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow,
        FString::Printf(TEXT("[서버] %s -> %s 거래 신청 전달 완료"), *GetPlayerNickname(), *TargetPC->GetPlayerNickname()));
  }
}

void ANonPlayerController::Client_ReceiveTradeRequestDirect_Implementation(const FString& RequesterNickname, ANonPlayerController* RequesterPC) {
  PendingTradeRequesterPC = RequesterPC;

  Client_AddSystemMessage(FString::Printf(TEXT("%s 님이 1:1 개인 거래를 신청했습니다. [수락: Y / 거절: N]"), *RequesterNickname));
  ShowInteractionRequest(EInteractionRequestType::TradeRequest, RequesterNickname, RequesterPC, 15.f);
}

bool ANonPlayerController::Server_RespondTradeRequestDirect_Validate(bool bAccept) {
  return true;
}

void ANonPlayerController::Server_RespondTradeRequestDirect_Implementation(bool bAccept) {
  if (PendingTradeRequesterPC.IsValid()) {
    if (bAccept) {
      Client_AddSystemMessage(FString::Printf(TEXT("%s 님과의 1:1 개인 거래를 시작합니다."), *PendingTradeRequesterPC->GetPlayerNickname()));
      PendingTradeRequesterPC->Client_AddSystemMessage(FString::Printf(TEXT("%s 님이 거래 신청을 수락했습니다."), *GetPlayerNickname()));
    } else {
      Client_AddSystemMessage(FString::Printf(TEXT("%s 님의 거래 신청을 거절했습니다."), *PendingTradeRequesterPC->GetPlayerNickname()));
      PendingTradeRequesterPC->Client_AddSystemMessage(FString::Printf(TEXT("%s 님이 거래 신청을 거절했습니다."), *GetPlayerNickname()));
    }

    if (UTradeComponent* MyTrade = GetTradeComponent()) {
      MyTrade->SetPendingRequesterPC(PendingTradeRequesterPC.Get());
      MyTrade->Server_RespondTradeRequest(bAccept);
    }
  }
  PendingTradeRequesterPC = nullptr;
}

bool ANonPlayerController::Server_SendPartyInviteByNickname_Validate(const FString& TargetNickname) {
  return true;
}

void ANonPlayerController::Server_SendPartyInviteByNickname_Implementation(const FString& TargetNickname) {
  const FString TrimmedName = TargetNickname.TrimStartAndEnd();
  if (GEngine) {
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Magenta,
        FString::Printf(TEXT("[서버 Nick수신] 초대대상='%s'"), *TrimmedName));
  }

  if (TrimmedName.IsEmpty()) {
    Client_AddSystemMessage(TEXT("초대할 플레이어의 닉네임을 입력해 주세요. (예: /파티초대 닉네임)"));
    return;
  }

  ANonPlayerController* TargetPC = nullptr;
  if (GetWorld()) {
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) {
      if (ANonPlayerController* NonPC = Cast<ANonPlayerController>(It->Get())) {
        const FString PCNick = NonPC->GetPlayerNickname().TrimStartAndEnd();
        FString CharName = TEXT("");
        if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(NonPC->GetPawn())) {
          CharName = Char->GetPlayerName().TrimStartAndEnd();
        }

        if (GEngine) {
          GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan,
              FString::Printf(TEXT("[후보] Nick='%s', Char='%s' vs '%s'"), *PCNick, *CharName, *TrimmedName));
        }

        if (PCNick.Equals(TrimmedName, ESearchCase::IgnoreCase) ||
            (!CharName.IsEmpty() && CharName.Equals(TrimmedName, ESearchCase::IgnoreCase))) {
          TargetPC = NonPC;
          break;
        }
      }
    }
  }

  if (!TargetPC) {
    Client_AddSystemMessage(FString::Printf(TEXT("'%s' 님을 찾을 수 없습니다. (오프라인이거나 이름이 다릅니다)"), *TrimmedName));
    return;
  }

  if (TargetPC == this) {
    Client_AddSystemMessage(TEXT("[시스템] 자기 자신은 파티에 초대할 수 없습니다."));
    return;
  }

  // 파티 초대 사전 유효성 검사 (이미 같은 파티, 다른 파티, 권한, 정원 초과 등)
  UPartyComponent* PartyComp = GetPartyComponent();
  if (!PartyComp) {
    PartyComp = FindComponentByClass<UPartyComponent>();
  }

  if (PartyComp) {
    FString FailReason;
    if (!PartyComp->CanInvite(TargetPC, FailReason)) {
      Client_AddSystemMessage(FailReason);
      return;
    }
    PartyComp->InvitePartyInternal(TargetPC);
  }

  if (GEngine) {
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
        FString::Printf(TEXT("[서버] 닉네임 파티 초대 전달: %s -> %s"), *GetPlayerNickname(), *TargetPC->GetPlayerNickname()));
  }

  // 1. 상대방 컨트롤러에 초대 알림 직통 전송 (RPC 전달 100% 보장)
  TargetPC->PendingPartyInviterPC = this;
  TargetPC->Client_ReceivePartyInviteDirect(GetPlayerNickname(), this);

  // 2. 초대를 보낸 나 자신에게 시스템 메시지 전송
  Client_AddSystemMessage(FString::Printf(TEXT("[시스템] %s 님에게 파티 초대를 보냈습니다."), *TargetPC->GetPlayerNickname()));
}

bool ANonPlayerController::Server_LeavePartyDirect_Validate() {
  return true;
}

void ANonPlayerController::Server_LeavePartyDirect_Implementation() {
  if (UPartyComponent* MyParty = GetPartyComponent()) {
    if (!MyParty->IsInParty()) {
      Client_AddSystemMessage(TEXT("[시스템] 현재 속해 있는 파티가 없습니다."));
      return;
    }

    MyParty->LeaveParty();
  }
}

bool ANonPlayerController::IsChatFocused() const
{
  if (FSlateApplication::IsInitialized())
  {
    TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
    if (FocusedWidget.IsValid())
    {
      const FString WidgetType = FocusedWidget->GetTypeAsString();
      if (WidgetType.Contains(TEXT("EditableText")) || WidgetType.Contains(TEXT("MultiLineEditableText")))
      {
        return true;
      }
    }
  }
  return false;
}

void ANonPlayerController::OnHotkeyAcceptRequest()
{
  if (!IsLocalController()) return;

  if (InteractionRequestWidget && InteractionRequestWidget->IsInViewport() && InteractionRequestWidget->GetVisibility() == ESlateVisibility::Visible)
  {
    if (!IsChatFocused())
    {
      InteractionRequestWidget->AcceptRequest();
    }
  }
}

void ANonPlayerController::OnHotkeyDeclineRequest()
{
  if (!IsLocalController()) return;

  if (InteractionRequestWidget && InteractionRequestWidget->IsInViewport() && InteractionRequestWidget->GetVisibility() == ESlateVisibility::Visible)
  {
    if (!IsChatFocused())
    {
      InteractionRequestWidget->DeclineRequest();
    }
  }
}