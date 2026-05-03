#include "Core/NonUIManagerComponent.h"
#include "Ability/NonAttributeSet.h" // [New] for RefreshHUDState
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Character/NonCharacterBase.h" // [New] for Casting
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Equipment/EquipmentComponent.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Skill/SkillManagerComponent.h"
#include "System/NonGameInstance.h" // [New]
#include "System/NonSaveGame.h"     // [New]
#include "UI/Character/CharacterWindowWidget.h"
#include "UI/DraggableWindowBase.h"
#include "UI/InGameHUD.h"
#include "UI/Inventory/InventorySlotWidget.h"
#include "UI/Inventory/InventoryWidget.h"
#include "UI/Skill/SkillWindowWidget.h"
#include "UI/System/SystemMenuWidget.h"
#include "UI/Dialogue/NonDialogueWidget.h"
#include "UI/Dialogue/NonDialogueChoiceWidget.h"
#include "UI/UIViewportUtils.h"

static int32 CountInventorySlotsDeep(UUserWidget *Root) {
  if (!Root)
    return 0;

  TSet<UUserWidget *> Visited;
  TArray<UUserWidget *> Stack;
  Stack.Add(Root);
  int32 Count = 0;

  while (Stack.Num() > 0) {
    UUserWidget *Cur = Stack.Pop(EAllowShrinking::No);
    if (!Cur || Visited.Contains(Cur))
      continue;
    Visited.Add(Cur);

    if (Cur->WidgetTree) {
      TArray<UWidget *> All;
      Cur->WidgetTree->GetAllWidgets(All);
      for (UWidget *W : All) {
        if (Cast<UInventorySlotWidget>(W)) {
          ++Count;
        } else if (UUserWidget *ChildUW = Cast<UUserWidget>(W)) {
          Stack.Add(ChildUW); // 자식 UserWidget 내부 트리도 검사
        }
      }
    }
  }
  return Count;
}

static UInventoryWidget *FindInventoryContent(UUserWidget *Root) {
  if (!Root)
    return nullptr;
  if (auto *AsInv = Cast<UInventoryWidget>(Root))
    return AsInv;

  if (UWidgetTree *WT = Root->WidgetTree) {
    TArray<UWidget *> All;
    WT->GetAllWidgets(All);
    for (UWidget *W : All)
      if (auto *IW = Cast<UInventoryWidget>(W))
        return IW;
  }
  return nullptr;
}

static UCharacterWindowWidget *FindCharacterContent(UUserWidget *Root) {
  if (!Root)
    return nullptr;
  if (auto *AsChar = Cast<UCharacterWindowWidget>(Root))
    return AsChar;

  if (UWidgetTree *WT = Root->WidgetTree) {
    TArray<UWidget *> All;
    WT->GetAllWidgets(All);
    for (UWidget *W : All) {
      if (auto *CW = Cast<UCharacterWindowWidget>(W)) {
        return CW; // 첫 번째로 찾은 캐릭터창 컨텐츠 반환
      }
    }
  }
  return nullptr;
}

UNonUIManagerComponent::UNonUIManagerComponent() {
  PrimaryComponentTick.bCanEverTick = false;
}

APlayerController *UNonUIManagerComponent::GetPC() const {
  if (const APawn *Pawn = Cast<APawn>(GetOwner())) {
    return Cast<APlayerController>(Pawn->GetController());
  }
  return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
}

void UNonUIManagerComponent::BeginPlay() {
  Super::BeginPlay();
  InitHUD();
}

void UNonUIManagerComponent::InitHUD() {
  if (!InGameHUDClass)
    return;

  // [New] 로비/타이틀 맵에서는 HUD를 생성하지 않음
  if (UWorld *World = GetWorld()) {
    FString MapName = World->GetMapName();
    MapName.RemoveFromStart(World->StreamingLevelsPrefix); // PIE 접두사 제거

    if (MapName.Contains(TEXT("Lobby")) || MapName.Contains(TEXT("Title"))) {
      return;
    }
  }

  APlayerController *PC = GetPC(); // 헬퍼 함수 사용

  // [Multiplayer & Standalone Fix]
  // GetPC()는 Owner 폰의 컨트롤러를 우선적으로 찾고, 없으면
  // GetFirstPlayerController를 반환합니다.
  if (!PC) {
    return; // 최후의 수단으로도 PC가 없으면 리턴
  }

  // 로컬 컨트롤러일 때만 HUD 생성 (멀티플레이 시 남의 폰에 내 HUD가 뜨는 것
  // 방지)
  if (PC->IsLocalController()) {
    InGameHUD = CreateWidget<UInGameHUD>(PC, InGameHUDClass);
    if (InGameHUD) {
      InGameHUD->AddToViewport();

      if (UNonGameInstance *GI =
              Cast<UNonGameInstance>(GetWorld()->GetGameInstance())) {
        // [Changed] 기본적으로 Live Data(Character)를 우선 사용합니다.
        EJobClass LiveJob = EJobClass::None;
        bool bFoundLive = false;

        if (APawn *MyOwner = Cast<APawn>(GetOwner())) {
          if (USkillManagerComponent *SkillMgr =
                  MyOwner->FindComponentByClass<USkillManagerComponent>()) {
            LiveJob = SkillMgr->GetJobClass();
            bFoundLive = true;

            // 변경 사항 감지를 위해 델리게이트 등록 (이미 등록되어 있을 수
            // 있으니 Unique)
            SkillMgr->OnJobChanged.AddUniqueDynamic(
                this, &UNonUIManagerComponent::OnJobChanged);
          }
        }

        if (bFoundLive) {
          UpdateClassIconFromJob(LiveJob);
        } else {
          // Fallback: 캐릭터에서 못 찾았으면 세이브 파일 확인
          if (!GI->CurrentSlotName.IsEmpty() &&
              UGameplayStatics::DoesSaveGameExist(GI->CurrentSlotName, 0)) {
            if (UNonSaveGame *Data =
                    Cast<UNonSaveGame>(UGameplayStatics::LoadGameFromSlot(
                        GI->CurrentSlotName, 0))) {
              UpdateClassIconFromJob(Data->JobClass);
            }
          }
        }
      }
    }
  }
}

void UNonUIManagerComponent::OnJobChanged(EJobClass NewJob) {
  UpdateClassIconFromJob(NewJob);
}

// [New] 외부(캐릭터/세이브시스템)에서 데이터 로드 완료 후 강제로 HUD 갱신을
// 요청할 때 사용
void UNonUIManagerComponent::RefreshHUDState() {
  if (ANonCharacterBase *MyChar = Cast<ANonCharacterBase>(GetOwner())) {
    // 1. 직업 아이콘 갱신
    if (USkillManagerComponent *SkillMgr =
            MyChar->FindComponentByClass<USkillManagerComponent>()) {
      // 현재 SkillMgr이 가지고 있는 직업으로 아이콘 강제 갱신
      UpdateClassIconFromJob(SkillMgr->GetJobClass());
    }

    // [New] 캐릭터 이름 갱신
    if (InGameHUD) {
      InGameHUD->UpdateCharacterName(MyChar->GetPlayerName());
    }

    // 2. 스탯(HP, MP, SP, Level, EXP) 갱신
    UAbilitySystemComponent *ASC = MyChar->GetAbilitySystemComponent();
    if (const UNonAttributeSet *AS =
            Cast<UNonAttributeSet>(MyChar->GetAttributeSet())) {
      UpdateHP(AS->GetHP(), AS->GetMaxHP());
      UpdateMP(AS->GetMP(), AS->GetMaxMP());
      UpdateSP(AS->GetSP(), AS->GetMaxSP());
      UpdateLevel((int32)AS->GetLevel());
      UpdateEXP(AS->GetExp(), AS->GetExpToNextLevel());

      // 3. 변경 감지 델리게이트 등록 (아직 안했다면)
      if (ASC && !bAttributeDelegatesBound) {
        BindAttributeChangeDelegates(ASC);
      }
    }
  }
}

void UNonUIManagerComponent::BindAttributeChangeDelegates(
    UAbilitySystemComponent *ASC) {
  if (!ASC)
    return;

  if (const UNonAttributeSet *AS = Cast<UNonAttributeSet>(
          ASC->GetAttributeSet(UNonAttributeSet::StaticClass()))) {
    ASC->GetGameplayAttributeValueChangeDelegate(AS->GetHPAttribute())
        .AddUObject(this, &UNonUIManagerComponent::OnHPChanged);
    ASC->GetGameplayAttributeValueChangeDelegate(AS->GetMaxHPAttribute())
        .AddUObject(this, &UNonUIManagerComponent::OnMaxHPChanged);
    ASC->GetGameplayAttributeValueChangeDelegate(AS->GetMPAttribute())
        .AddUObject(this, &UNonUIManagerComponent::OnMPChanged);
    ASC->GetGameplayAttributeValueChangeDelegate(AS->GetMaxMPAttribute())
        .AddUObject(this, &UNonUIManagerComponent::OnMaxMPChanged);
    ASC->GetGameplayAttributeValueChangeDelegate(AS->GetSPAttribute())
        .AddUObject(this, &UNonUIManagerComponent::OnSPChanged);
    ASC->GetGameplayAttributeValueChangeDelegate(AS->GetMaxSPAttribute())
        .AddUObject(this, &UNonUIManagerComponent::OnMaxSPChanged);
    ASC->GetGameplayAttributeValueChangeDelegate(AS->GetExpAttribute())
        .AddUObject(this, &UNonUIManagerComponent::OnExpChanged);
    ASC->GetGameplayAttributeValueChangeDelegate(
           AS->GetExpToNextLevelAttribute())
        .AddUObject(this, &UNonUIManagerComponent::OnMaxExpChanged);
    ASC->GetGameplayAttributeValueChangeDelegate(AS->GetLevelAttribute())
        .AddUObject(this, &UNonUIManagerComponent::OnLevelChanged);

    bAttributeDelegatesBound = true;
  }
}

void UNonUIManagerComponent::OnHPChanged(const FOnAttributeChangeData &Data) {
  if (ANonCharacterBase *MyChar = Cast<ANonCharacterBase>(GetOwner())) {
    if (const UNonAttributeSet *AS =
            Cast<UNonAttributeSet>(MyChar->GetAttributeSet())) {
      UpdateHP(Data.NewValue, AS->GetMaxHP());
    }
  }
}

void UNonUIManagerComponent::OnMaxHPChanged(
    const FOnAttributeChangeData &Data) {
  if (ANonCharacterBase *MyChar = Cast<ANonCharacterBase>(GetOwner())) {
    if (const UNonAttributeSet *AS =
            Cast<UNonAttributeSet>(MyChar->GetAttributeSet())) {
      UpdateHP(AS->GetHP(), Data.NewValue);
    }
  }
}

void UNonUIManagerComponent::OnMPChanged(const FOnAttributeChangeData &Data) {
  if (ANonCharacterBase *MyChar = Cast<ANonCharacterBase>(GetOwner())) {
    if (const UNonAttributeSet *AS =
            Cast<UNonAttributeSet>(MyChar->GetAttributeSet())) {
      UpdateMP(Data.NewValue, AS->GetMaxMP());
    }
  }
}

void UNonUIManagerComponent::OnMaxMPChanged(
    const FOnAttributeChangeData &Data) {
  if (ANonCharacterBase *MyChar = Cast<ANonCharacterBase>(GetOwner())) {
    if (const UNonAttributeSet *AS =
            Cast<UNonAttributeSet>(MyChar->GetAttributeSet())) {
      UpdateMP(AS->GetMP(), Data.NewValue);
    }
  }
}

void UNonUIManagerComponent::OnSPChanged(const FOnAttributeChangeData &Data) {
  if (ANonCharacterBase *MyChar = Cast<ANonCharacterBase>(GetOwner())) {
    if (const UNonAttributeSet *AS =
            Cast<UNonAttributeSet>(MyChar->GetAttributeSet())) {
      UpdateSP(Data.NewValue, AS->GetMaxSP());
    }
  }
}

void UNonUIManagerComponent::OnMaxSPChanged(
    const FOnAttributeChangeData &Data) {
  if (ANonCharacterBase *MyChar = Cast<ANonCharacterBase>(GetOwner())) {
    if (const UNonAttributeSet *AS =
            Cast<UNonAttributeSet>(MyChar->GetAttributeSet())) {
      UpdateSP(AS->GetSP(), Data.NewValue);
    }
  }
}

void UNonUIManagerComponent::OnExpChanged(const FOnAttributeChangeData &Data) {
  if (ANonCharacterBase *MyChar = Cast<ANonCharacterBase>(GetOwner())) {
    if (const UNonAttributeSet *AS =
            Cast<UNonAttributeSet>(MyChar->GetAttributeSet())) {
      UpdateEXP(Data.NewValue, AS->GetExpToNextLevel());
    }
  }
}

void UNonUIManagerComponent::OnMaxExpChanged(
    const FOnAttributeChangeData &Data) {
  if (ANonCharacterBase *MyChar = Cast<ANonCharacterBase>(GetOwner())) {
    if (const UNonAttributeSet *AS =
            Cast<UNonAttributeSet>(MyChar->GetAttributeSet())) {
      UpdateEXP(AS->GetExp(), Data.NewValue);
    }
  }
}

void UNonUIManagerComponent::OnLevelChanged(
    const FOnAttributeChangeData &Data) {
  UpdateLevel((int32)Data.NewValue);
}

void UNonUIManagerComponent::UpdateClassIconFromJob(EJobClass Job) {
  if (InGameHUD) {
    if (UTexture2D **FoundIcon = ClassIcons.Find(Job)) {
      InGameHUD->UpdateClassIcon(*FoundIcon);
    } else {
      // 아이콘이 없으면 숨김/기본 처리
      InGameHUD->UpdateClassIcon(nullptr);
    }
  }
}

// ----- HUD bridge -----
void UNonUIManagerComponent::UpdateHP(float Current, float Max) {
  if (InGameHUD)
    InGameHUD->UpdateHP(Current, Max);
}
void UNonUIManagerComponent::UpdateMP(float Current, float Max) {
  if (InGameHUD)
    InGameHUD->UpdateMP(Current, Max);
}
void UNonUIManagerComponent::UpdateSP(float Current, float Max) {
  if (InGameHUD)
    InGameHUD->UpdateSP(Current, Max);
}
void UNonUIManagerComponent::UpdateEXP(float Current, float Max) {
  if (InGameHUD)
    InGameHUD->UpdateEXP(Current, Max);
}
void UNonUIManagerComponent::UpdateLevel(int32 NewLevel) {
  if (InGameHUD) {
    InGameHUD->UpdateLevel(NewLevel);
  }
}

void UNonUIManagerComponent::UpdateTargetHUD(AActor *TargetActor,
                                             const FString &Name, float HP,
                                             float MaxHP, float Distance) {
  if (InGameHUD) {

    bool bShow = (TargetActor != nullptr);
    InGameHUD->UpdateTargetInfo(bShow, Name, HP, MaxHP, Distance);
  } else {
  }
}
// ========================= Inventory =========================
void UNonUIManagerComponent::ShowInventory() {
  OpenWindow(EGameWindowType::Inventory);
}

void UNonUIManagerComponent::HideInventory() {
  CloseWindow(EGameWindowType::Inventory);
}

bool UNonUIManagerComponent::IsInventoryVisible() const {
  if (!InventoryWidget.IsValid())
    return false;

  const ESlateVisibility Vis = InventoryWidget->GetVisibility();
  return InventoryWidget->IsInViewport() && Vis != ESlateVisibility::Hidden &&
         Vis != ESlateVisibility::Collapsed;
}

UUserWidget *UNonUIManagerComponent::GetInventoryWidget() const {
  return GetWindow(EGameWindowType::Inventory);
}

void UNonUIManagerComponent::ToggleInventory() {
  if (!IsInventoryVisible())
    ShowInventory();
  else
    HideInventory();
}

// ========================= Character =========================
void UNonUIManagerComponent::ShowCharacter() {
  OpenWindow(EGameWindowType::Character);
}

void UNonUIManagerComponent::HideCharacter() {
  CloseWindow(EGameWindowType::Character);
}

void UNonUIManagerComponent::ToggleCharacter() {
  if (!CharacterWidget.IsValid() || !CharacterWidget->IsInViewport() ||
      CharacterWidget->GetVisibility() == ESlateVisibility::Collapsed ||
      CharacterWidget->GetVisibility() == ESlateVisibility::Hidden) {
    ShowCharacter();
  } else {
    HideCharacter();
  }
}

bool UNonUIManagerComponent::IsCharacterVisible() const {
  if (!CharacterWidget.IsValid())
    return false;
  const ESlateVisibility Vis = CharacterWidget->GetVisibility();
  return CharacterWidget->IsInViewport() && Vis != ESlateVisibility::Hidden &&
         Vis != ESlateVisibility::Collapsed;
}

UUserWidget *UNonUIManagerComponent::GetCharacterWidget() const {
  return GetWindow(EGameWindowType::Character);
}

// ========================= Input Mode =========================
void UNonUIManagerComponent::SetUIInputMode(bool bShowCursor /*=false*/) {
  if (APlayerController *PC = GetPC()) {
    bool bWasVisible = PC->bShowMouseCursor;
    PC->bShowMouseCursor = bShowCursor;

    if (bShowCursor) {
      FInputModeGameAndUI Mode;
      Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
      Mode.SetHideCursorDuringCapture(false);
      PC->SetInputMode(Mode);

      PC->bEnableClickEvents = true;
      PC->bEnableMouseOverEvents = true;

      // 커서가 새로 나타나는 시점이라면 중앙 정렬
      if (!bWasVisible) {
        int32 SizeX, SizeY;
        PC->GetViewportSize(SizeX, SizeY);
        PC->SetMouseLocation(SizeX / 2, SizeY / 2);
      }
    } else {
      PC->SetInputMode(FInputModeGameOnly{});
      PC->bEnableClickEvents = false;
      PC->bEnableMouseOverEvents = false;
    }

    PC->SetIgnoreMoveInput(false);
    PC->SetIgnoreLookInput(false);
  }
}

void UNonUIManagerComponent::SetGameInputMode() {
  if (APlayerController *PC = GetPC()) {
    PC->bShowMouseCursor = false;
    PC->SetInputMode(FInputModeGameOnly{});

    PC->bEnableClickEvents = false;
    PC->bEnableMouseOverEvents = false;

    PC->SetIgnoreMoveInput(false);
    PC->SetIgnoreLookInput(false);
  }
}

// ========================= Window Register / Z =========================
void UNonUIManagerComponent::RegisterWindow(UUserWidget *Window) {
  if (!Window)
    return;

  // 목록 순서 갱신 (항상 맨 뒤로 = 최상단)
  // 이미 목록에 있다면 제거 후 다시 추가하여 순서를 최신으로 유지
  const int32 Idx = OpenWindows.IndexOfByPredicate(
      [Window](const TWeakObjectPtr<UUserWidget> &Item) {
        return Item.Get() == Window;
      });

  if (Idx != INDEX_NONE) {
    OpenWindows.RemoveAt(Idx);
  }
  OpenWindows.Add(Window);

  const bool bAlreadyInViewport = Window->IsInViewport();
  UDraggableWindowBase *Draggable = Cast<UDraggableWindowBase>(Window);

  if (!bAlreadyInViewport) {
    // 처음 뷰포트에 붙일 때
    Window->AddToViewport(++TopZOrder);

    if (Draggable) {
      // 저장된 위치가 있으면 그 위치, 없으면 기본 위치
      const FVector2D TargetPos = Draggable->HasSavedViewportPos()
                                      ? Draggable->GetSavedViewportPos()
                                      : Draggable->GetDefaultViewportPos();

      Window->SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
      Window->SetAlignmentInViewport(FVector2D(0.f, 0.f));
      Window->SetPositionInViewport(TargetPos, /*bRemoveDPIScale=*/false);

      Draggable->SetSavedViewportPos(TargetPos);
    }
  } else {
    // 이미 뷰포트에 있으면 ZOrder만 올리고,
    // 위치는 우리가 저장해둔 좌표로 그대로 복원 (지오메트리 사용 X)
    Window->RemoveFromParent();
    Window->AddToViewport(++TopZOrder);

    if (Draggable) {
      const FVector2D TargetPos = Draggable->GetSavedViewportPos();

      Window->SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
      Window->SetAlignmentInViewport(FVector2D(0.f, 0.f));
      Window->SetPositionInViewport(TargetPos, /*bRemoveDPIScale=*/false);
    }
  }

  // 창이 하나라도 열리면 커서 보이기 (단, 상호작용 프롬프트 등은 제외해야 할
  // 수도 있음) 일단 RegisterWindow를 타는 창들은(인벤, 캐릭터, 시스템메뉴 등)
  // 모두 커서가 필요한 창들임.
  SetUIInputMode(true);
}

void UNonUIManagerComponent::BringToFront(UUserWidget *Window) {
  // RegisterWindow 안에서 ZOrder 올리고 위치 복원까지 다 처리
  RegisterWindow(Window);
}

void UNonUIManagerComponent::UnregisterWindow(UUserWidget *Window) {
  if (!Window)
    return;

  OpenWindows.Remove(Window);

  if (!IsAnyWindowVisible()) {
    SetGameInputMode();
  }
}

// ========================= Utils =========================
bool UNonUIManagerComponent::IsAnyWindowVisible() const {
  if (IsInventoryVisible())
    return true;
  if (IsCharacterVisible())
    return true;

  // [New] 대화창이 켜져있어도 UI 모드 유지
  if (DialogueWidget && DialogueWidget->IsInViewport() && 
      DialogueWidget->GetVisibility() != ESlateVisibility::Hidden && 
      DialogueWidget->GetVisibility() != ESlateVisibility::Collapsed) {
      return true;
  }

  for (const TWeakObjectPtr<UUserWidget> &W : OpenWindows) {
    if (W.IsValid()) {
      const ESlateVisibility V = W->GetVisibility();
      if (W->IsInViewport() && V != ESlateVisibility::Hidden &&
          V != ESlateVisibility::Collapsed) {
        return true;
      }
    }
  }
  return false;
}

bool UNonUIManagerComponent::GetWidgetViewportPos(UUserWidget *W,
                                                  FVector2D &OutViewportPos) {
  OutViewportPos = FVector2D::ZeroVector;
  if (!W)
    return false;

  const FGeometry &Geo = W->GetCachedGeometry();
  FVector2D PixelPos(0.f), ViewPos(0.f);
  USlateBlueprintLibrary::AbsoluteToViewport(
      W->GetWorld(), Geo.GetAbsolutePosition(), PixelPos, ViewPos);

  if (!ViewPos.IsNearlyZero()) {
    OutViewportPos = ViewPos;
    return true;
  }
  return false;
}

void UNonUIManagerComponent::NotifyWindowClosed(UUserWidget *Window) {
  if (!Window)
    return;

  Window->SetVisibility(ESlateVisibility::Collapsed);
  OpenWindows.Remove(Window);

  if (!IsAnyWindowVisible()) {
    SetGameInputMode();
  }
}

void UNonUIManagerComponent::RefreshCharacterEquipmentUI() {
  if (!CharacterWidget.IsValid())
    return;

  if (UCharacterWindowWidget *C = FindCharacterContent(CharacterWidget.Get())) {
    C->RefreshAllSlots(); // ← 슬롯 전부 즉시 갱신
  }
}

// Skill Window
void UNonUIManagerComponent::ToggleSkillWindow() {
  ToggleWindow(EGameWindowType::Skill);
}

void UNonUIManagerComponent::ShowSkillWindow() {
  OpenWindow(EGameWindowType::Skill);
}

void UNonUIManagerComponent::HideSkillWindow() {
  CloseWindow(EGameWindowType::Skill);
}

void UNonUIManagerComponent::ShowInteractPrompt(const FText &InLabel) {
  if (!InteractPromptClass) {
    return;
  }

  // 아직 안 만들어졌으면 생성
  if (!InteractPromptWidget) {
    APlayerController *PC = nullptr;

    // 1) Owner가 Pawn이면 그 컨트롤러 사용
    if (APawn *PawnOwner = Cast<APawn>(GetOwner())) {
      PC = Cast<APlayerController>(PawnOwner->GetController());
    }

    // 2) 그래도 없으면 월드에서 첫 번째 플레이어 가져오기 (안전망)
    if (!PC && GetWorld()) {
      PC = GetWorld()->GetFirstPlayerController();
    }

    if (PC) {
      InteractPromptWidget = CreateWidget<UUserWidget>(PC, InteractPromptClass);
      if (InteractPromptWidget) {
        InteractPromptWidget->AddToViewport(1000);
      }
    }
  }

  if (InteractPromptWidget) {
    // 텍스트 갱신 (WBP_InteractPrompt 안 TextBlock 이름을 "Text_Label" 로
    // 맞춰둔 경우)
    if (UTextBlock *LabelText = Cast<UTextBlock>(
            InteractPromptWidget->GetWidgetFromName(TEXT("Text_Label")))) {
      LabelText->SetText(InLabel);
    }

    InteractPromptWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
  }
}

void UNonUIManagerComponent::HideInteractPrompt() {
  if (InteractPromptWidget) {
    InteractPromptWidget->SetVisibility(ESlateVisibility::Collapsed);
  }
}

bool UNonUIManagerComponent::CloseTopWindow() {
  // 역순으로 순회하며 최상단 Visible 창 찾기
  for (int32 i = OpenWindows.Num() - 1; i >= 0; --i) {
    UUserWidget *W = OpenWindows[i].Get();
    if (!W) {
      OpenWindows.RemoveAt(i);
      continue;
    }

    // 화면에 보이고 있는 창만 대상
    if (W->IsInViewport() &&
        W->GetVisibility() != ESlateVisibility::Collapsed &&
        W->GetVisibility() != ESlateVisibility::Hidden) {
      // 1) 인벤토리
      if (InventoryWidget.IsValid() && W == InventoryWidget.Get()) {
        HideInventory();
        return true;
      }
      // 2) 캐릭터
      if (CharacterWidget.IsValid() && W == CharacterWidget.Get()) {
        HideCharacter();
        return true;
      }
      // 3) 스킬
      if (SkillWindow && W == SkillWindow) {
        HideSkillWindow();
        return true;
      }

      // 4) 그 외 (Draggable 등)
      // 그냥 닫고 리스트에서 제거
      W->SetVisibility(ESlateVisibility::Collapsed);
      UnregisterWindow(W);
      return true;
    }
  }

  return false; // 닫을 창이 없었음
}
// ========================= Generic Window Logic =========================

UUserWidget *UNonUIManagerComponent::GetWindow(EGameWindowType Type) const {
  if (const TObjectPtr<UUserWidget> *Found = ManagedWindows.Find(Type)) {
    return Found->Get();
  }
  return nullptr;
}

bool UNonUIManagerComponent::IsWindowOpen(EGameWindowType Type) const {
  UUserWidget *W = GetWindow(Type);
  if (!W)
    return false;

  return W->IsInViewport() &&
         W->GetVisibility() != ESlateVisibility::Collapsed &&
         W->GetVisibility() != ESlateVisibility::Hidden;
}

void UNonUIManagerComponent::ToggleWindow(EGameWindowType Type) {
  if (IsWindowOpen(Type)) {
    CloseWindow(Type);
  } else {
    OpenWindow(Type);
  }
}

void UNonUIManagerComponent::OpenWindow(EGameWindowType Type) {
  APlayerController *PC = GetPC();
  if (!PC)
    return;

  UUserWidget *Window = GetWindow(Type);

  // 1. 없으면 생성
  if (!Window) {
    TSubclassOf<UUserWidget> Class = GetWindowClass(Type);
    if (!Class)
      return; // 클래스 설정 안됨

    Window = CreateWidget<UUserWidget>(PC, Class);
    if (!Window)
      return;

    ManagedWindows.Add(Type, Window);

    // 공통 & 개별 초기화
    RegisterWindow(Window);
    SetupWindow(Type, Window);
  } else {
    // 2. 있으면 맨 앞으로
    if (!Window->IsInViewport()) {
      RegisterWindow(Window);
    } else {
      BringToFront(Window);
    }
    SetupWindow(Type, Window);
  }

  Window->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

  // (Legacy Sync)
  if (Type == EGameWindowType::Inventory)
    InventoryWidget = Window;
  if (Type == EGameWindowType::Character)
    CharacterWidget = Window;
  if (Type == EGameWindowType::Skill)
    SkillWindow = Window;
}

void UNonUIManagerComponent::CloseWindow(EGameWindowType Type) {
  UUserWidget *Window = GetWindow(Type);
  if (Window && Window->IsInViewport()) {
    Window->SetVisibility(ESlateVisibility::Collapsed);
    UnregisterWindow(Window);
  }

  if (!IsAnyWindowVisible()) {
    SetGameInputMode();
  }
}

TSubclassOf<UUserWidget>
UNonUIManagerComponent::GetWindowClass(EGameWindowType Type) const {
  switch (Type) {
  case EGameWindowType::Inventory:
    return InventoryWidgetClass;
  case EGameWindowType::Character:
    return CharacterWidgetClass;
  case EGameWindowType::Skill:
    return SkillWindowClass;
  case EGameWindowType::SystemMenu:
    return SystemMenuWidgetClass;
  default:
    return nullptr;
  }
}

void UNonUIManagerComponent::SetupWindow(EGameWindowType Type,
                                         UUserWidget *Widget) {
  if (!Widget)
    return;

  APlayerController *PC = GetPC();
  AActor *Owner = GetOwner();
  APawn *Pawn = Cast<APawn>(Owner);

  auto FindInvComp = [&]() -> UInventoryComponent * {
    UInventoryComponent *C = Owner->FindComponentByClass<UInventoryComponent>();
    if (!C && Pawn)
      C = Pawn->FindComponentByClass<UInventoryComponent>();
    if (!C && PC && PC->GetPawn())
      C = PC->GetPawn()->FindComponentByClass<UInventoryComponent>();
    return C;
  };
  auto FindEqComp = [&]() -> UEquipmentComponent * {
    UEquipmentComponent *C = Owner->FindComponentByClass<UEquipmentComponent>();
    if (!C && Pawn)
      C = Pawn->FindComponentByClass<UEquipmentComponent>();
    if (!C && PC)
      C = PC->FindComponentByClass<UEquipmentComponent>();
    if (!C && PC && PC->GetPawn())
      C = PC->GetPawn()->FindComponentByClass<UEquipmentComponent>();
    return C;
  };
  auto FindSkillMgr = [&]() -> USkillManagerComponent * {
    if (Pawn)
      return Pawn->FindComponentByClass<USkillManagerComponent>();
    if (PC && PC->GetPawn())
      return PC->GetPawn()->FindComponentByClass<USkillManagerComponent>();
    return nullptr;
  };

  switch (Type) {
  case EGameWindowType::Inventory: {
    if (UInventoryWidget *InvUI = FindInventoryContent(Widget)) {
      InvUI->InitInventoryUI(FindInvComp(), FindEqComp());
    }
  } break;

  case EGameWindowType::Character: {
    if (UCharacterWindowWidget *CW = FindCharacterContent(Widget)) {
      CW->InitCharacterUI(FindInvComp(), FindEqComp());
    }
  } break;

  case EGameWindowType::Skill: {
    USkillManagerComponent *Mgr = FindSkillMgr();
    USkillDataAsset *DA = nullptr;
    // [Changed] UIManager에서는 더 이상 DA를 관리하지 않음 (SkillManager에
    // 위임)
    /*
    if (Mgr)
    {
        if (USkillDataAsset** FoundDA = SkillDataByJob.Find(Mgr->GetJobClass()))
        {
            DA = *FoundDA;
        }
    }
    */

    USkillWindowWidget *Content = nullptr;
    if (UWidgetTree *Tree = Widget->WidgetTree) {
      TArray<UWidget *> All;
      Tree->GetAllWidgets(All);
      for (UWidget *W : All) {
        if (USkillWindowWidget *SW = Cast<USkillWindowWidget>(W)) {
          Content = SW;
          break;
        }
      }
    }
    if (!Content)
      Content = Cast<USkillWindowWidget>(Widget);

    if (Content) {
      SkillWindowContent = Content;
      Content->Init(Mgr, DA);
    }
  } break;

  case EGameWindowType::SystemMenu: {
    if (USystemMenuWidget *SysMenu = Cast<USystemMenuWidget>(Widget)) {
      SysMenu->Init(this);
    }
  } break;

  default:
    break;
  }
}

// ========================= Dialogue UI =========================
void UNonUIManagerComponent::ShowDialogue(const FText& NPCName, const FText& DialogueLine) {
    if (!DialogueWidgetClass) {
        return;
    }

    bool bJustCreated = false;
    if (!DialogueWidget) {
        if (APlayerController* PC = GetPC()) {
            DialogueWidget = CreateWidget<UNonDialogueWidget>(PC, DialogueWidgetClass);
            if (DialogueWidget) {
                DialogueWidget->AddToViewport(2000);
                bJustCreated = true;
            }
        }
    }

    if (DialogueWidget) {
        DialogueWidget->SetDialogueData(NPCName, DialogueLine);
        
        bool bNeedsAnim = bJustCreated || !DialogueWidget->IsVisible();
        DialogueWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

        // 위젯이 Visible 상태가 된 직후에 애니메이션을 재생해야 씹히지 않습니다.
        if (bNeedsAnim) {
            DialogueWidget->PlayFadeIn();
        }
        
        // [New] 대화창이 열리면 마우스 커서를 표시하고 UI 입력 모드로 전환
        SetUIInputMode(true);

        // 키보드 입력을 받을 수 있도록 포커스 부여
        DialogueWidget->SetFocus();

        // [New] 대화 중에는 방해되지 않도록 HUD 숨김
        if (InGameHUD) {
            InGameHUD->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void UNonUIManagerComponent::HideDialogue() {
    if (DialogueWidget) {
        DialogueWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
    
    // [New] 대화가 끝나면 HUD 다시 표시
    if (InGameHUD) {
        InGameHUD->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    // [New] 열려있는 다른 창이 없다면 게임 모드로 복귀 (마우스 커서 숨김)
    if (!IsAnyWindowVisible()) {
        SetGameInputMode();
    }
}

void UNonUIManagerComponent::StartDialogue(UDataTable* DialogueTable, FName StartingNodeID, const FText& TargetNPCName)
{
    if (!DialogueTable || StartingNodeID.IsNone())
    {
        return;
    }

    CurrentDialogueTable = DialogueTable;
    CurrentDialogueNodeID = StartingNodeID;
    CurrentDialogueNPCName = TargetNPCName;

    // 첫 번째 대사로 진행
    AdvanceDialogue();
}

void UNonUIManagerComponent::AdvanceDialogue()
{
    if (!CurrentDialogueTable || CurrentDialogueNodeID.IsNone())
    {
        // 대화 종료
        if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(GetOwner()))
        {
            Player->EndDialogueCamera();
        }
        return;
    }

    // 데이터 테이블에서 현재 ID의 Row를 찾습니다.
    FString ContextString = TEXT("DialogueLookup");
    FDialogueRow* Row = CurrentDialogueTable->FindRow<FDialogueRow>(CurrentDialogueNodeID, ContextString);

    if (Row)
    {
        // 화자 이름 결정 (Row에 이름이 비어있으면 NPC의 기본 이름 사용)
        FText NameToShow = Row->NPCName.IsEmpty() ? CurrentDialogueNPCName : Row->NPCName;

        // 화면에 대화창 띄우기
        ShowDialogue(NameToShow, Row->DialogueText);

        if (DialogueWidget)
        {
            DialogueWidget->ClearChoices();

            // 선택지가 있다면 버튼들을 생성
            if (Row->Choices.Num() > 0)
            {
                if (DialogueChoiceWidgetClass && GetPC())
                {
                    int32 ChoiceIndex = 1;
                    for (const FDialogueChoice& ChoiceData : Row->Choices)
                    {
                        UNonDialogueChoiceWidget* ChoiceWidget = CreateWidget<UNonDialogueChoiceWidget>(GetPC(), DialogueChoiceWidgetClass);
                        if (ChoiceWidget)
                        {
                            ChoiceWidget->SetupChoice(ChoiceData, this, ChoiceIndex);
                            DialogueWidget->AddChoiceWidget(ChoiceWidget);
                        }
                        ChoiceIndex++;
                    }
                }
            }
            
            // 다음 대사 준비
            CurrentDialogueNodeID = Row->NextNodeID;
        }
    }
    else
    {
        // 매칭되는 Row가 없다면 대화 종료
        CurrentDialogueNodeID = NAME_None;
        AdvanceDialogue();
    }
}

void UNonUIManagerComponent::OnDialogueChoiceSelected(const FDialogueChoice& Choice)
{
    // 액션이 있다면 실행 (예: "OpenShop")
    if (!Choice.ActionToTrigger.IsNone() && Choice.ActionToTrigger != FName("None"))
    {
        FString ActionStr = Choice.ActionToTrigger.ToString();
        if (ActionStr == TEXT("OpenShop"))
        {
            // 상점 열기 로직
            // 예: OpenWindow(EGameWindowType::Shop);
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Action: 상점을 엽니다!"));
            }
        }
        else if (ActionStr == TEXT("EndDialogue"))
        {
            CurrentDialogueNodeID = NAME_None;
        }
    }

    // 다음 대사 ID가 지정되어 있다면 그 대사로 넘어감
    if (!Choice.NextNodeID.IsNone() && Choice.NextNodeID != FName("None"))
    {
        CurrentDialogueNodeID = Choice.NextNodeID;
        AdvanceDialogue();
    }
    else
    {
        // 다음 대사가 없으면 대화 종료
        CurrentDialogueNodeID = NAME_None;
        AdvanceDialogue();
    }
}