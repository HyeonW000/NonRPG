#include "Core/BPC_UIManager.h"
#include "UI/InGameHUD.h"
#include "UI/CharacterWindowWidget.h"
#include "UI/InventorySlotWidget.h"
#include "UI/InventoryWidget.h"
#include "UI/SkillWindowWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/UIViewportUtils.h"
#include "UI/DraggableWindowBase.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Inventory/InventoryComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "Skill/SkillManagerComponent.h"


static int32 CountInventorySlotsDeep(UUserWidget* Root)
{
    if (!Root) return 0;

    TSet<UUserWidget*> Visited;
    TArray<UUserWidget*> Stack; Stack.Add(Root);
    int32 Count = 0;

    while (Stack.Num() > 0)
    {
        UUserWidget* Cur = Stack.Pop(EAllowShrinking::No);
        if (!Cur || Visited.Contains(Cur)) continue;
        Visited.Add(Cur);

        if (Cur->WidgetTree)
        {
            TArray<UWidget*> All;
            Cur->WidgetTree->GetAllWidgets(All);
            for (UWidget* W : All)
            {
                if (Cast<UInventorySlotWidget>(W))
                {
                    ++Count;
                }
                else if (UUserWidget* ChildUW = Cast<UUserWidget>(W))
                {
                    Stack.Add(ChildUW); // 자식 UserWidget 내부 트리도 검사
                }
            }
        }
    }
    return Count;
}

static UInventoryWidget* FindInventoryContent(UUserWidget* Root)
{
    if (!Root) return nullptr;
    if (auto* AsInv = Cast<UInventoryWidget>(Root)) return AsInv;

    if (UWidgetTree* WT = Root->WidgetTree)
    {
        TArray<UWidget*> All; WT->GetAllWidgets(All);
        for (UWidget* W : All)
            if (auto* IW = Cast<UInventoryWidget>(W))
                return IW;
    }
    return nullptr;
}

static UCharacterWindowWidget* FindCharacterContent(UUserWidget* Root)
{
    if (!Root) return nullptr;
    if (auto* AsChar = Cast<UCharacterWindowWidget>(Root)) return AsChar;

    if (UWidgetTree* WT = Root->WidgetTree)
    {
        TArray<UWidget*> All;
        WT->GetAllWidgets(All);
        for (UWidget* W : All)
        {
            if (auto* CW = Cast<UCharacterWindowWidget>(W))
            {
                return CW; // 첫 번째로 찾은 캐릭터창 컨텐츠 반환
            }
        }
    }
    return nullptr;
}

UBPC_UIManager::UBPC_UIManager()
{
    PrimaryComponentTick.bCanEverTick = false;
}

APlayerController* UBPC_UIManager::GetPC() const
{
    if (const APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        return Cast<APlayerController>(Pawn->GetController());
    }
    return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
}

void UBPC_UIManager::BeginPlay()
{
    Super::BeginPlay();
    InitHUD();
}

void UBPC_UIManager::InitHUD()
{
    if (!InGameHUDClass) return;

    if (APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
        {
            InGameHUD = CreateWidget<UInGameHUD>(PC, InGameHUDClass);
            if (InGameHUD)
            {
                InGameHUD->AddToViewport();
            }
        }
    }
}

// ----- HUD bridge -----
void UBPC_UIManager::UpdateHP(float Current, float Max) { if (InGameHUD) InGameHUD->UpdateHP(Current, Max); }
void UBPC_UIManager::UpdateMP(float Current, float Max) { if (InGameHUD) InGameHUD->UpdateMP(Current, Max); }
void UBPC_UIManager::UpdateSP(float Current, float Max) { if (InGameHUD) InGameHUD->UpdateSP(Current, Max); }
void UBPC_UIManager::UpdateEXP(float Current, float Max) { if (InGameHUD) InGameHUD->UpdateEXP(Current, Max); }
void UBPC_UIManager::UpdateLevel(int32 NewLevel) { if (InGameHUD) InGameHUD->UpdateLevel(NewLevel); }

// ========================= Inventory =========================
void UBPC_UIManager::ShowInventory()
{
    APlayerController* PC = GetPC();
    if (!PC) return;

    if (!InventoryWidget.IsValid())
    {
        if (!InventoryWidgetClass) return;

        UUserWidget* W = CreateWidget<UUserWidget>(PC, InventoryWidgetClass);
        if (!W) return;

        InventoryWidget = W;

        // 창 등록 + 기본 위치 적용 (DraggableWindowBase 의 DefaultViewportPos)
        RegisterWindow(W);

        // ===== 인벤토리 / 장비 컴포넌트 주입 =====
        if (UInventoryWidget* InvUI = FindInventoryContent(InventoryWidget.Get()))
        {
            UInventoryComponent* InvComp = nullptr;
            UEquipmentComponent* EqComp = nullptr;

            if (AActor* OwnerActor = GetOwner())
            {
                InvComp = OwnerActor->FindComponentByClass<UInventoryComponent>();
                EqComp = OwnerActor->FindComponentByClass<UEquipmentComponent>();
            }

            if ((!InvComp || !EqComp) && PC)
            {
                if (APawn* Pawn = PC->GetPawn())
                {
                    if (!InvComp) InvComp = Pawn->FindComponentByClass<UInventoryComponent>();
                    if (!EqComp)  EqComp = Pawn->FindComponentByClass<UEquipmentComponent>();
                }
            }

            InvUI->InitInventoryUI(InvComp, EqComp);
            UE_LOG(LogTemp, Log,
                TEXT("[UIManager] InitInventoryUI -> Inv=%s, Eq=%s"),
                *GetNameSafe(InvComp), *GetNameSafe(EqComp));
        }
    }
    else
    {
        // 이미 만들어져 있으면 위치는 그대로, ZOrder만 맨 앞으로
        BringToFront(InventoryWidget.Get());
    }

    if (InventoryWidget.IsValid())
    {
        InventoryWidget->SetVisibility(ESlateVisibility::Visible);
    }
}

void UBPC_UIManager::HideInventory()
{
    if (InventoryWidget.IsValid())
    {
        InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (!IsAnyWindowVisible())
    {
        SetGameInputMode();
    }
}


bool UBPC_UIManager::IsInventoryVisible() const
{
    if (!InventoryWidget.IsValid()) return false;

    const ESlateVisibility Vis = InventoryWidget->GetVisibility();
    return InventoryWidget->IsInViewport()
        && Vis != ESlateVisibility::Hidden
        && Vis != ESlateVisibility::Collapsed;
}

UUserWidget* UBPC_UIManager::GetInventoryWidget() const
{
    return InventoryWidget.Get();
}

void UBPC_UIManager::ToggleInventory()
{
    if (!IsInventoryVisible())
        ShowInventory();
    else
        HideInventory();
}

// ========================= Character =========================
void UBPC_UIManager::ShowCharacter()
{
    APlayerController* PC = GetPC();
    if (!PC) return;

    if (!CharacterWidget.IsValid())
    {
        if (!CharacterWidgetClass) return;

        UUserWidget* W = CreateWidget<UUserWidget>(PC, CharacterWidgetClass);
        if (!W) return;

        CharacterWidget = W;
        RegisterWindow(W);
    }
    else
    {
        BringToFront(CharacterWidget.Get());
    }

    // ===== 캐릭터창에 인벤토리 / 장비 컴포넌트 주입 =====
    UInventoryComponent* InvComp = nullptr;
    UEquipmentComponent* EqComp = nullptr;

    if (AActor* Owner = GetOwner())
    {
        InvComp = Owner->FindComponentByClass<UInventoryComponent>();
        EqComp = Owner->FindComponentByClass<UEquipmentComponent>();

        if (APawn* Pawn = Cast<APawn>(Owner))
        {
            if (!InvComp) InvComp = Pawn->FindComponentByClass<UInventoryComponent>();
            if (!EqComp)  EqComp = Pawn->FindComponentByClass<UEquipmentComponent>();

            if (APlayerController* PCon = Cast<APlayerController>(Pawn->GetController()))
            {
                if (!EqComp) EqComp = PCon->FindComponentByClass<UEquipmentComponent>();
                if (!InvComp && PCon->GetPawn())
                    InvComp = PCon->GetPawn()->FindComponentByClass<UInventoryComponent>();
                if (!EqComp && PCon->GetPawn())
                    EqComp = PCon->GetPawn()->FindComponentByClass<UEquipmentComponent>();
            }
        }
    }

    if (CharacterWidget.IsValid())
    {
        if (UCharacterWindowWidget* CW = FindCharacterContent(CharacterWidget.Get()))
        {
            CW->InitCharacterUI(InvComp, EqComp);
            UE_LOG(LogTemp, Log,
                TEXT("[UIManager] InitCharacterUI -> Inv=%s, Eq=%s"),
                *GetNameSafe(InvComp), *GetNameSafe(EqComp));
        }

        CharacterWidget->SetVisibility(ESlateVisibility::Visible);
    }
}

void UBPC_UIManager::HideCharacter()
{
    if (CharacterWidget.IsValid())
    {
        CharacterWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (!IsAnyWindowVisible())
    {
        SetGameInputMode();
    }
}

void UBPC_UIManager::ToggleCharacter()
{
    if (!CharacterWidget.IsValid() || !CharacterWidget->IsInViewport()
        || CharacterWidget->GetVisibility() == ESlateVisibility::Collapsed
        || CharacterWidget->GetVisibility() == ESlateVisibility::Hidden)
    {
        ShowCharacter();
    }
    else
    {
        HideCharacter();
    }
}

bool UBPC_UIManager::IsCharacterVisible() const
{
    if (!CharacterWidget.IsValid()) return false;
    const ESlateVisibility Vis = CharacterWidget->GetVisibility();
    return CharacterWidget->IsInViewport()
        && Vis != ESlateVisibility::Hidden
        && Vis != ESlateVisibility::Collapsed;
}

UUserWidget* UBPC_UIManager::GetCharacterWidget() const
{
    return CharacterWidget.Get();
}

// ========================= Input Mode =========================
void UBPC_UIManager::SetUIInputMode(bool bShowCursor /*=false*/)
{
    if (APlayerController* PC = GetPC())
    {
        PC->bShowMouseCursor = bShowCursor;

        if (bShowCursor)
        {
            FInputModeGameAndUI Mode;
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Mode.SetHideCursorDuringCapture(false);
            PC->SetInputMode(Mode);

            PC->bEnableClickEvents = true;
            PC->bEnableMouseOverEvents = true;
        }
        else
        {
            PC->SetInputMode(FInputModeGameOnly{});
            PC->bEnableClickEvents = false;
            PC->bEnableMouseOverEvents = false;
        }

        PC->SetIgnoreMoveInput(false);
        PC->SetIgnoreLookInput(false);
    }
}

void UBPC_UIManager::SetGameInputMode()
{
    if (APlayerController* PC = GetPC())
    {
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly{});

        PC->bEnableClickEvents = false;
        PC->bEnableMouseOverEvents = false;

        PC->SetIgnoreMoveInput(false);
        PC->SetIgnoreLookInput(false);
    }
}

// ========================= Window Register / Z =========================
void UBPC_UIManager::RegisterWindow(UUserWidget* Window)
{
    if (!Window) return;

    // 목록에 없으면 추가
    if (!OpenWindows.Contains(Window))
    {
        OpenWindows.Add(Window);
    }

    const bool bAlreadyInViewport = Window->IsInViewport();
    UDraggableWindowBase* Draggable = Cast<UDraggableWindowBase>(Window);

    if (!bAlreadyInViewport)
    {
        // 처음 뷰포트에 붙일 때
        Window->AddToViewport(++TopZOrder);

        if (Draggable)
        {
            // 저장된 위치가 있으면 그 위치, 없으면 기본 위치
            const FVector2D TargetPos = Draggable->HasSavedViewportPos()
                ? Draggable->GetSavedViewportPos()
                : Draggable->GetDefaultViewportPos();

            Window->SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
            Window->SetAlignmentInViewport(FVector2D(0.f, 0.f));
            Window->SetPositionInViewport(TargetPos, /*bRemoveDPIScale=*/false);

            Draggable->SetSavedViewportPos(TargetPos);
        }
    }
    else
    {
        // 이미 뷰포트에 있으면 ZOrder만 올리고,
        // 위치는 우리가 저장해둔 좌표로 그대로 복원 (지오메트리 사용 X)
        Window->RemoveFromParent();
        Window->AddToViewport(++TopZOrder);

        if (Draggable)
        {
            const FVector2D TargetPos = Draggable->GetSavedViewportPos();

            Window->SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
            Window->SetAlignmentInViewport(FVector2D(0.f, 0.f));
            Window->SetPositionInViewport(TargetPos, /*bRemoveDPIScale=*/false);
        }
    }

    bool bCursor = false;
    if (APlayerController* PC = GetPC())
    {
        bCursor = PC->bShowMouseCursor;
    }
    SetUIInputMode(bCursor);
}

void UBPC_UIManager::BringToFront(UUserWidget* Window)
{
    // RegisterWindow 안에서 ZOrder 올리고 위치 복원까지 다 처리
    RegisterWindow(Window);
}

void UBPC_UIManager::UnregisterWindow(UUserWidget* Window)
{
    if (!Window) return;

    OpenWindows.Remove(Window);

    if (!IsAnyWindowVisible())
    {
        SetGameInputMode();
    }
}

// ========================= Utils =========================
bool UBPC_UIManager::IsAnyWindowVisible() const
{
    if (IsInventoryVisible())
        return true;
    if (IsCharacterVisible())
        return true;

    for (const TWeakObjectPtr<UUserWidget>& W : OpenWindows)
    {
        if (W.IsValid())
        {
            const ESlateVisibility V = W->GetVisibility();
            if (W->IsInViewport() && V != ESlateVisibility::Hidden && V != ESlateVisibility::Collapsed)
            {
                return true;
            }
        }
    }
    return false;
}

bool UBPC_UIManager::GetWidgetViewportPos(UUserWidget* W, FVector2D& OutViewportPos)
{
    OutViewportPos = FVector2D::ZeroVector;
    if (!W) return false;

    const FGeometry& Geo = W->GetCachedGeometry();
    FVector2D PixelPos(0.f), ViewPos(0.f);
    USlateBlueprintLibrary::AbsoluteToViewport(W->GetWorld(), Geo.GetAbsolutePosition(), PixelPos, ViewPos);

    if (!ViewPos.IsNearlyZero())
    {
        OutViewportPos = ViewPos;
        return true;
    }
    return false;
}

void UBPC_UIManager::NotifyWindowClosed(UUserWidget* Window)
{
    if (!Window) return;

    Window->SetVisibility(ESlateVisibility::Collapsed);
    OpenWindows.Remove(Window);

    if (!IsAnyWindowVisible())
    {
        SetGameInputMode();
    }
}

void UBPC_UIManager::RefreshCharacterEquipmentUI()
{
    if (!CharacterWidget.IsValid()) return;

    if (UCharacterWindowWidget* C = FindCharacterContent(CharacterWidget.Get()))
    {
        C->RefreshAllSlots(); // ← 슬롯 전부 즉시 갱신
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] RefreshCharacterEquipmentUI -> RefreshAllSlots()"));
    }
}

//Skill Window
void UBPC_UIManager::ToggleSkillWindow()
{
    if (SkillWindow && SkillWindow->IsInViewport() &&
        SkillWindow->GetVisibility() != ESlateVisibility::Collapsed &&
        SkillWindow->GetVisibility() != ESlateVisibility::Hidden)
    {
        HideSkillWindow();
    }
    else
    {
        ShowSkillWindow();
    }
}

void UBPC_UIManager::ShowSkillWindow()
{
    APlayerController* PC = GetPC();
    if (!PC || !SkillWindowClass) return;

    // 1) 바깥 창 생성 (WBP_SkillWindow : DraggableWindowBase 자식)
    if (!SkillWindow)
    {
        SkillWindow = CreateWidget<UUserWidget>(PC, SkillWindowClass);
        if (!SkillWindow) return;

        RegisterWindow(SkillWindow);   // 위치 설정

        // ★ 이름이 아니라 타입(USkillWindowWidget) 기준으로 안쪽 WBP_Skill 찾기
        if (!SkillWindowContent)
        {
            if (UWidgetTree* Tree = SkillWindow->WidgetTree)
            {
                TArray<UWidget*> AllWidgets;
                Tree->GetAllWidgets(AllWidgets);

                for (UWidget* W : AllWidgets)
                {
                    if (USkillWindowWidget* SW = Cast<USkillWindowWidget>(W))
                    {
                        SkillWindowContent = SW;
                        break;
                    }
                }
            }

            if (!SkillWindowContent)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("[UIManager] SkillWindowContent not found (no USkillWindowWidget in SkillWindow)"));
            }
        }
    }
    else
    {
        if (!SkillWindow->IsInViewport())
        {
            RegisterWindow(SkillWindow);
        }
        else
        {
            BringToFront(SkillWindow);
        }
    }

    // 2) 직업별 스킬 DA 연결
    USkillManagerComponent* Mgr = nullptr;
    USkillDataAsset* DA = nullptr;

    if (APawn* P = PC->GetPawn())
    {
        Mgr = P->FindComponentByClass<USkillManagerComponent>();
        if (Mgr)
        {
            const EJobClass Job = Mgr->GetJobClass();
            DA = SkillDataByJob.FindRef(Job);
        }
    }

    // 3) 실제 스킬창(안쪽 USkillWindowWidget)에 Init 호출
    if (SkillWindowContent)
    {
        SkillWindowContent->Init(Mgr, DA);
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[UIManager] SkillWindowContent is null (Init not called)"));
    }

    SkillWindow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    UE_LOG(LogTemp, Warning, TEXT("[UIManager] ShowSkillWindow: SkillMgr = %s"),
        Mgr ? TEXT("VALID") : TEXT("NULL"));
}


void UBPC_UIManager::HideSkillWindow()
{
    if (SkillWindow && SkillWindow->IsInViewport())
    {
        SkillWindow->SetVisibility(ESlateVisibility::Collapsed);
        UnregisterWindow(SkillWindow);
    }

    if (!IsAnyWindowVisible())
    {
        SetGameInputMode();
    }
}