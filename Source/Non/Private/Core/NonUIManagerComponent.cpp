#include "Core/NonUIManagerComponent.h"
#include "UI/InGameHUD.h"
#include "UI/Character/CharacterWindowWidget.h"
#include "UI/Inventory/InventorySlotWidget.h"
#include "UI/Inventory/InventoryWidget.h"
#include "UI/Skill/SkillWindowWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
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

UNonUIManagerComponent::UNonUIManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

APlayerController* UNonUIManagerComponent::GetPC() const
{
    if (const APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        return Cast<APlayerController>(Pawn->GetController());
    }
    return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
}

void UNonUIManagerComponent::BeginPlay()
{
    Super::BeginPlay();
    InitHUD();
}

void UNonUIManagerComponent::InitHUD()
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
void UNonUIManagerComponent::UpdateHP(float Current, float Max) { if (InGameHUD) InGameHUD->UpdateHP(Current, Max); }
void UNonUIManagerComponent::UpdateMP(float Current, float Max) { if (InGameHUD) InGameHUD->UpdateMP(Current, Max); }
void UNonUIManagerComponent::UpdateSP(float Current, float Max) { if (InGameHUD) InGameHUD->UpdateSP(Current, Max); }
void UNonUIManagerComponent::UpdateEXP(float Current, float Max) { if (InGameHUD) InGameHUD->UpdateEXP(Current, Max); }
void UNonUIManagerComponent::UpdateLevel(int32 NewLevel) { if (InGameHUD) InGameHUD->UpdateLevel(NewLevel); }

// ========================= Inventory =========================
void UNonUIManagerComponent::ShowInventory()
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

void UNonUIManagerComponent::HideInventory()
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


bool UNonUIManagerComponent::IsInventoryVisible() const
{
    if (!InventoryWidget.IsValid()) return false;

    const ESlateVisibility Vis = InventoryWidget->GetVisibility();
    return InventoryWidget->IsInViewport()
        && Vis != ESlateVisibility::Hidden
        && Vis != ESlateVisibility::Collapsed;
}

UUserWidget* UNonUIManagerComponent::GetInventoryWidget() const
{
    return InventoryWidget.Get();
}

void UNonUIManagerComponent::ToggleInventory()
{
    if (!IsInventoryVisible())
        ShowInventory();
    else
        HideInventory();
}

// ========================= Character =========================
void UNonUIManagerComponent::ShowCharacter()
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
        }

        CharacterWidget->SetVisibility(ESlateVisibility::Visible);
    }
}

void UNonUIManagerComponent::HideCharacter()
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

void UNonUIManagerComponent::ToggleCharacter()
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

bool UNonUIManagerComponent::IsCharacterVisible() const
{
    if (!CharacterWidget.IsValid()) return false;
    const ESlateVisibility Vis = CharacterWidget->GetVisibility();
    return CharacterWidget->IsInViewport()
        && Vis != ESlateVisibility::Hidden
        && Vis != ESlateVisibility::Collapsed;
}

UUserWidget* UNonUIManagerComponent::GetCharacterWidget() const
{
    return CharacterWidget.Get();
}

// ========================= Input Mode =========================
void UNonUIManagerComponent::SetUIInputMode(bool bShowCursor /*=false*/)
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

void UNonUIManagerComponent::SetGameInputMode()
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
void UNonUIManagerComponent::RegisterWindow(UUserWidget* Window)
{
    if (!Window) return;

    // 목록 순서 갱신 (항상 맨 뒤로 = 최상단)
    // 이미 목록에 있다면 제거 후 다시 추가하여 순서를 최신으로 유지
    const int32 Idx = OpenWindows.IndexOfByPredicate([Window](const TWeakObjectPtr<UUserWidget>& Item) {
        return Item.Get() == Window;
    });
    
    if (Idx != INDEX_NONE)
    {
        OpenWindows.RemoveAt(Idx);
    }
    OpenWindows.Add(Window);

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

void UNonUIManagerComponent::BringToFront(UUserWidget* Window)
{
    // RegisterWindow 안에서 ZOrder 올리고 위치 복원까지 다 처리
    RegisterWindow(Window);
}

void UNonUIManagerComponent::UnregisterWindow(UUserWidget* Window)
{
    if (!Window) return;

    OpenWindows.Remove(Window);

    if (!IsAnyWindowVisible())
    {
        SetGameInputMode();
    }
}

// ========================= Utils =========================
bool UNonUIManagerComponent::IsAnyWindowVisible() const
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

bool UNonUIManagerComponent::GetWidgetViewportPos(UUserWidget* W, FVector2D& OutViewportPos)
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

void UNonUIManagerComponent::NotifyWindowClosed(UUserWidget* Window)
{
    if (!Window) return;

    Window->SetVisibility(ESlateVisibility::Collapsed);
    OpenWindows.Remove(Window);

    if (!IsAnyWindowVisible())
    {
        SetGameInputMode();
    }
}

void UNonUIManagerComponent::RefreshCharacterEquipmentUI()
{
    if (!CharacterWidget.IsValid()) return;

    if (UCharacterWindowWidget* C = FindCharacterContent(CharacterWidget.Get()))
    {
        C->RefreshAllSlots(); // ← 슬롯 전부 즉시 갱신
    }
}

//Skill Window
void UNonUIManagerComponent::ToggleSkillWindow()
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

void UNonUIManagerComponent::ShowSkillWindow()
{
    APlayerController* PC = GetPC();
    if (!PC || !SkillWindowClass) return;

    // 1) 바깥 창 생성 (WBP_SkillWindow : DraggableWindowBase 자식)
    if (!SkillWindow)
    {
        SkillWindow = CreateWidget<UUserWidget>(PC, SkillWindowClass);
        if (!SkillWindow) return;

        RegisterWindow(SkillWindow);   // 위치 설정

        //  이름이 아니라 타입(USkillWindowWidget) 기준으로 안쪽 WBP_Skill 찾기
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
    }
    SkillWindow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}


void UNonUIManagerComponent::HideSkillWindow()
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

void UNonUIManagerComponent::ShowInteractPrompt(const FText& InLabel)
{
    if (!InteractPromptClass)
    {
        return;
    }

    // 아직 안 만들어졌으면 생성
    if (!InteractPromptWidget)
    {
        APlayerController* PC = nullptr;

        // 1) Owner가 Pawn이면 그 컨트롤러 사용
        if (APawn* PawnOwner = Cast<APawn>(GetOwner()))
        {
            PC = Cast<APlayerController>(PawnOwner->GetController());
        }

        // 2) 그래도 없으면 월드에서 첫 번째 플레이어 가져오기 (안전망)
        if (!PC && GetWorld())
        {
            PC = GetWorld()->GetFirstPlayerController();
        }

        if (PC)
        {
            InteractPromptWidget = CreateWidget<UUserWidget>(PC, InteractPromptClass);
            if (InteractPromptWidget)
            {
                InteractPromptWidget->AddToViewport(1000);
            }
        }
    }

    if (InteractPromptWidget)
    {
        // 텍스트 갱신 (WBP_InteractPrompt 안 TextBlock 이름을 "Text_Label" 로 맞춰둔 경우)
        if (UTextBlock* LabelText = Cast<UTextBlock>(InteractPromptWidget->GetWidgetFromName(TEXT("Text_Label"))))
        {
            LabelText->SetText(InLabel);
        }

        InteractPromptWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

void UNonUIManagerComponent::HideInteractPrompt()
{
    if (InteractPromptWidget)
    {
        InteractPromptWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

bool UNonUIManagerComponent::CloseTopWindow()
{
    // 역순으로 순회하며 최상단 Visible 창 찾기
    for (int32 i = OpenWindows.Num() - 1; i >= 0; --i)
    {
        UUserWidget* W = OpenWindows[i].Get();
        if (!W)
        {
            OpenWindows.RemoveAt(i);
            continue;
        }

        // 화면에 보이고 있는 창만 대상
        if (W->IsInViewport() && 
            W->GetVisibility() != ESlateVisibility::Collapsed && 
            W->GetVisibility() != ESlateVisibility::Hidden)
        {
            // 1) 인벤토리
            if (InventoryWidget.IsValid() && W == InventoryWidget.Get())
            {
                HideInventory();
                return true;
            }
            // 2) 캐릭터
            if (CharacterWidget.IsValid() && W == CharacterWidget.Get())
            {
                HideCharacter();
                return true;
            }
            // 3) 스킬
            if (SkillWindow && W == SkillWindow)
            {
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