#include "Core/BPC_UIManager.h"
#include "UI/InGameHUD.h"
#include "UI/CharacterWindowWidget.h"
#include "UI/InventorySlotWidget.h"
#include "UI/InventoryWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Inventory/InventoryComponent.h"
#include "Equipment/EquipmentComponent.h"



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

        W->AddToViewport();
        InventoryWidget = W;
        RegisterWindow(W);

        // 처음엔 숨겼다가 끝에서 Visible
        InventoryWidget->SetVisibility(ESlateVisibility::Hidden);

        // --- 여기서 바로 자식 UInventoryWidget 찾아서 C++ 초기화만 수행 ---
        if (UInventoryWidget* InvUI = FindInventoryContent(InventoryWidget.Get()))
        {
            UInventoryComponent* InvComp = nullptr;
            UEquipmentComponent* EqComp = nullptr;

            // Owner → Pawn/Controller 순으로 컴포넌트 탐색
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

            // 핵심: 자식 UInventoryWidget에 직접 주입
            InvUI->InitInventoryUI(InvComp, EqComp);
            UE_LOG(LogTemp, Warning, TEXT("[UIManager] InitInventoryUI(InventoryWidget) -> Inv=%s, Eq=%s"),
                *GetNameSafe(InvComp), *GetNameSafe(EqComp));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[UIManager] No UInventoryWidget under InventoryWidget"));
        }
    }
    else
    {
        if (!InventoryWidget->IsInViewport())
        {
            InventoryWidget->AddToViewport();
        }
    }

    // 공통 UI 정리
    BringToFront(InventoryWidget.Get());

    FVector2D TargetPos = InventoryDefaultPos;
    if (SavedViewportPositions.Contains(InventoryWidget))
    {
        TargetPos = SavedViewportPositions[InventoryWidget];
    }
    QueueSetViewportPosition(InventoryWidget.Get(), TargetPos);

    InventoryWidget->InvalidateLayoutAndVolatility();
    InventoryWidget->ForceLayoutPrepass();
    InventoryWidget->SetDesiredSizeInViewport(InventoryWidget->GetDesiredSize());

    // 보이기
    InventoryWidget->SetVisibility(ESlateVisibility::Visible);

    // 입력 모드
    const bool bCursor = PC->bShowMouseCursor;
    SetUIInputMode(bCursor);
}



void UBPC_UIManager::HideInventory()
{
    if (InventoryWidget.IsValid())
    {
        FVector2D CurrPos;
        if (GetWidgetViewportPos(InventoryWidget.Get(), CurrPos))
        {
            SavedViewportPositions.Add(InventoryWidget, CurrPos);
        }

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

        W->AddToViewport(110);
        CharacterWidget = W;
        RegisterWindow(W);

        // 초기엔 잠깐 숨겼다가 끝에서 Visible로
        CharacterWidget->SetVisibility(ESlateVisibility::Hidden);
    }
    else if (!CharacterWidget->IsInViewport())
    {
        CharacterWidget->AddToViewport(110);
        CharacterWidget->ForceLayoutPrepass();
    }

    // ===== 여기서 Inventory / Equipment 찾아서 UI에 주입 =====
    UInventoryComponent* InvComp = nullptr;
    UEquipmentComponent* EqComp = nullptr;

    if (AActor* Owner = GetOwner())
    {
        // 우선 Owner에서
        InvComp = Owner->FindComponentByClass<UInventoryComponent>();
        EqComp = Owner->FindComponentByClass<UEquipmentComponent>();

        // Owner가 Pawn이면 Pawn/Controller 경로도 확인
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

    if (UCharacterWindowWidget* CW = FindCharacterContent(CharacterWidget.Get()))
    {
        CW->InitCharacterUI(InvComp, EqComp);
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] InitCharacterUI -> Inv=%s, Eq=%s"),
            *GetNameSafe(InvComp), *GetNameSafe(EqComp));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] No UCharacterWindowWidget under CharacterWidget"));
    }

    // ===== 공통 UI 정리 =====
    BringToFront(CharacterWidget.Get());
    CharacterWidget->ForceLayoutPrepass();
    const FVector2D Desired = CharacterWidget->GetDesiredSize();
    CharacterWidget->SetDesiredSizeInViewport(Desired);
    CharacterWidget->SetAlignmentInViewport(FVector2D(0.f, 0.f));

    FVector2D TargetPos = FVector2D(900.f, 200.f);
    if (SavedViewportPositions.Contains(CharacterWidget))
    {
        TargetPos = SavedViewportPositions[CharacterWidget];
    }
    QueueSetViewportPosition(CharacterWidget.Get(), TargetPos);
    CharacterWidget->SetVisibility(ESlateVisibility::Visible);

    const bool bCursor = PC->bShowMouseCursor;
    SetUIInputMode(bCursor);
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

    if (!OpenWindows.Contains(Window))
    {
        OpenWindows.Add(Window);
    }

    if (!Window->IsInViewport())
    {
        Window->AddToViewport(++TopZOrder);
    }
    else
    {
        //  SetZOrderInViewport 미지원 → 재부착으로 Z 갱신
        Window->RemoveFromParent();
        Window->AddToViewport(++TopZOrder);
    }

    // (좌표 복원은 기존처럼)
    const FGeometry& Geo = Window->GetCachedGeometry();
    FVector2D PixelPos(0.f), ViewportPos(0.f);
    USlateBlueprintLibrary::AbsoluteToViewport(Window->GetWorld(), Geo.GetAbsolutePosition(), PixelPos, ViewportPos);
    if (!ViewportPos.IsNearlyZero())
    {
        Window->SetAlignmentInViewport(FVector2D(0.f, 0.f));
        Window->SetPositionInViewport(ViewportPos, false);
    }

    bool bCursor = false;
    if (APlayerController* PC = GetPC()) { bCursor = PC->bShowMouseCursor; }
    SetUIInputMode(bCursor);
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

void UBPC_UIManager::BringToFront(UUserWidget* Window)
{
    if (!Window) return;

    if (!Window->IsInViewport())
    {
        Window->AddToViewport(++TopZOrder);
    }
    else
    {
        // ★ SetZOrderInViewport 미지원 → 재부착으로 Z 갱신
        Window->RemoveFromParent();
        Window->AddToViewport(++TopZOrder);
    }

    const FGeometry& Geo = Window->GetCachedGeometry();
    FVector2D PixelPos(0.f), ViewportPos(0.f);
    USlateBlueprintLibrary::AbsoluteToViewport(Window->GetWorld(), Geo.GetAbsolutePosition(), PixelPos, ViewportPos);
    if (!ViewportPos.IsNearlyZero())
    {
        Window->SetAlignmentInViewport(FVector2D(0.f, 0.f));
        Window->SetPositionInViewport(ViewportPos, false);
    }

    bool bCursor = false;
    if (APlayerController* PC = GetPC()) { bCursor = PC->bShowMouseCursor; }
    SetUIInputMode(bCursor);
}



// ========================= Deferred Position =========================
void UBPC_UIManager::QueueSetViewportPosition(UUserWidget* Window, const FVector2D& ViewportPos)
{
    if (!Window) return;

    DesiredViewportPositions.Add(Window, ViewportPos);

    if (!bDeferredPosScheduled)
    {
        bDeferredPosScheduled = true;

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimerForNextTick(
                FTimerDelegate::CreateUObject(this, &UBPC_UIManager::ApplyQueuedViewportPositions)
            );
        }
    }
}

void UBPC_UIManager::ApplyQueuedViewportPositions()
{
    bDeferredPosScheduled = false;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DeferredPosTimerHandle);
    }

    for (auto It = DesiredViewportPositions.CreateIterator(); It; ++It)
    {
        if (UUserWidget* W = It.Key().Get())
        {
            if (!W->IsInViewport())
            {
                W->AddToViewport(++TopZOrder);
            }
            else
            {
                // ★ SetZOrderInViewport 미지원 → 재부착으로 Z 갱신
                W->RemoveFromParent();
                W->AddToViewport(++TopZOrder);
            }

            W->SetAlignmentInViewport(FVector2D(0.f, 0.f));
            W->SetPositionInViewport(It.Value(), false);
            W->SetVisibility(ESlateVisibility::Visible);
        }
    }

    DesiredViewportPositions.Empty();
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

    FVector2D CurrPos;
    if (GetWidgetViewportPos(Window, CurrPos))
    {
        SavedViewportPositions.Add(Window, CurrPos);
    }

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