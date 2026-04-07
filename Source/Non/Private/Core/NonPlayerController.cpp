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
#include "Interaction/NonInteractableInterface.h"
#include "UI/CharacterCreationWidget.h" // [New]
#include "UI/CharacterSelectWidget.h"
#include "UI/QuickSlot/QuickSlotManager.h"

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

ANonPlayerController::ANonPlayerController() { bShowMouseCursor = false; }

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

  // [New] 로컬 클라이언트가 새 폰에 빙의했을 때, 자신의 장비/위치 정보를 서버에
  // 알림
  if (InPawn && IsLocalController()) {
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
}

void ANonPlayerController::SetupInputComponent() {
  Super::SetupInputComponent();

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

void ANonPlayerController::OnMove(const FInputActionValue &Value) {
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
        FGameplayTag::RequestGameplayTag(TEXT("State.Dodge"));

    static const FGameplayTag AttackTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Attack"));

    static const FGameplayTag ComboActiveTag =
        FGameplayTag::RequestGameplayTag(TEXT("Ability.Active.Combo"));

    // ⬇ 새로 추가
    static const FGameplayTag SkillTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Skill"));

    static const FGameplayTag GuardTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Guard"));

    if (ASC->HasMatchingGameplayTag(DodgeTag) ||
        ASC->HasMatchingGameplayTag(AttackTag) ||
        ASC->HasMatchingGameplayTag(ComboActiveTag) ||
        ASC->HasMatchingGameplayTag(SkillTag) ||
        ASC->HasMatchingGameplayTag(GuardTag)) {
      return; // 점프 안 함
    }
  }

  // 점프 막는중 아니면 평소처럼 점프
  CachedChar->Jump();
}

void ANonPlayerController::OnJumpStop(const FInputActionValue & /*Value*/) {
  if (CachedChar)
    CachedChar->StopJumping();
}

void ANonPlayerController::OnInteract(
    const FInputActionInstance & /*Instance*/) {
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
        FGameplayTag::RequestGameplayTag(TEXT("Ability.Active.Combo"));

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
      SetShowMouseCursor(false);
      FInputModeGameOnly GameOnly;
      SetInputMode(GameOnly);
      bEnableClickEvents = false;
      bEnableMouseOverEvents = false;
    }
    return;
  }

  if (CachedChar)
    CachedChar->HandleAttackInput(Value);
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

  // 캐릭터 캡슐 정보 가져오기
  UCapsuleComponent *Capsule = CachedChar->GetCapsuleComponent();
  if (!Capsule)
    return;

  const float Radius = Capsule->GetScaledCapsuleRadius();
  const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
  const float TraceDistance = 100.f;

  // 캡슐 중심 기준에서 앞쪽으로 스윕
  FVector Start = CachedChar->GetActorLocation();
  FVector End = Start + CachedChar->GetActorForwardVector() * TraceDistance;

  FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

  FHitResult Hit;
  FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractFocus), false,
                               CachedChar);

  const bool bHit = GetWorld()->SweepSingleByChannel(
      Hit, Start, End, FQuat::Identity, InteractChannel, Shape, Params);

  // #if WITH_EDITOR
  // const FColor Col = bHit ? FColor::Green : FColor::Red;
  // DrawDebugCapsule(GetWorld(), Start, HalfHeight, Radius, FQuat::Identity,
  // Col, false, 0.f); DrawDebugCapsule(GetWorld(), End, HalfHeight, Radius,
  // FQuat::Identity, Col, false, 0.f); DrawDebugLine(GetWorld(), Start, End,
  // Col, false, 0.f, 0, 1.f);
  // #endif

  AActor *NewTarget = nullptr;
  if (bHit && Hit.GetActor() &&
      Hit.GetActor()->Implements<UNonInteractableInterface>()) {
    NewTarget = Hit.GetActor();
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
    if (!bInPlace) {
      // 가장 가까운 PlayerStart 찾기
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
    }
    
    // 캐릭터 부활 시킴
    Char->Revive(bInPlace);
    
    // 사망 UI 닫기 및 입력 모드 복귀
    ClientOnSpawnFinished(); 
  }
}