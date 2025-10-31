#include "UI/QuickSlotBarWidget.h"
#include "UI/QuickSlotSlotWidget.h"
#include "UI/QuickSlotManager.h"
#include "Inventory/InventoryItem.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Character/NonCharacterBase.h" 

void UQuickSlotBarWidget::NativeConstruct()
{
    Super::NativeConstruct();

    RetryCount = 0;
    // 지연 초기화 시작
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().SetTimer(InitTimerHandle, this, &UQuickSlotBarWidget::TryInit, 0.05f, false);
    }
}

void UQuickSlotBarWidget::NativeDestruct()
{
    UnbindManagerDelegate();

    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearTimer(InitTimerHandle);
    }
    Super::NativeDestruct();
}

void UQuickSlotBarWidget::TryInit()
{
    // 1) Owning Player → Pawn → Character
    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        // 재시도
        if (++RetryCount < MaxRetries && GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &UQuickSlotBarWidget::TryInit, 0.05f, false);
        }
        return;
    }

    APawn* Pawn = PC->GetPawn();
    ANonCharacterBase* Char = Pawn ? Cast<ANonCharacterBase>(Pawn) : nullptr;
    if (!Char)
    {
        if (++RetryCount < MaxRetries && GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &UQuickSlotBarWidget::TryInit, 0.05f, false);
        }
        return;
    }

    // 2) QuickSlotManager 확보
    UQuickSlotManager* Mgr = Char->GetQuickSlotManager();
    if (!IsValid(Mgr))
    {
        if (++RetryCount < MaxRetries && GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &UQuickSlotBarWidget::TryInit, 0.05f, false);
        }
        return;
    }
    Manager = Mgr;

    // 3) 슬롯 수집
    if (!CollectSlots() || Slots.Num() == 0)
    {
        // 슬롯이 아직 생성되지 않았을 수 있음 → 재시도
        if (++RetryCount < MaxRetries && GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimer(InitTimerHandle, this, &UQuickSlotBarWidget::TryInit, 0.05f, false);
        }
        return;
    }

    // 4) 슬롯에 매니저 주입
    AssignManagerToSlots();

    // 5) 초기 동기화(아이콘/수량)
    InitialRefresh();

    // 6) 이벤트 바인딩(마지막!)
    BindManagerDelegate();
}

bool UQuickSlotBarWidget::CollectSlots()
{
    Slots.Reset();

    auto PushIfValid = [this](UQuickSlotSlotWidget* In)
        {
            if (IsValid(In))
            {
                Slots.Add(In);
            }
        };

    PushIfValid(Slot_0); PushIfValid(Slot_1); PushIfValid(Slot_2); PushIfValid(Slot_3); PushIfValid(Slot_4);
    PushIfValid(Slot_5); PushIfValid(Slot_6); PushIfValid(Slot_7); PushIfValid(Slot_8); PushIfValid(Slot_9);

    // 필요한 만큼만 수집(디자이너에서 일부 슬롯을 안 쓸 수도 있음)
    return true;
}

void UQuickSlotBarWidget::AssignManagerToSlots()
{
    if (!Manager.IsValid()) return;

    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (UQuickSlotSlotWidget* S = Slots[i])
        {
            S->SetManager(Manager.Get());
            S->QuickIndex = i;          // ★ 무조건 i로 덮어쓰기
        }
    }
}

void UQuickSlotBarWidget::InitialRefresh()
{
    for (UQuickSlotSlotWidget* S : Slots)
    {
        if (IsValid(S))
        {
            S->Refresh(); // 내부에서 Manager.ResolveItem(QuickIndex) + UpdateVisual 호출
        }
    }
}

void UQuickSlotBarWidget::BindManagerDelegate()
{
    if (!Manager.IsValid()) return;
    // 중복 바인딩 방지
    Manager->OnQuickSlotChanged.RemoveDynamic(this, &UQuickSlotBarWidget::HandleQuickSlotChanged);
    Manager->OnQuickSlotChanged.AddDynamic(this, &UQuickSlotBarWidget::HandleQuickSlotChanged);
}

void UQuickSlotBarWidget::UnbindManagerDelegate()
{
    if (Manager.IsValid())
    {
        Manager->OnQuickSlotChanged.RemoveDynamic(this, &UQuickSlotBarWidget::HandleQuickSlotChanged);
    }
}

void UQuickSlotBarWidget::HandleQuickSlotChanged(int32 SlotIndex, UInventoryItem* Item)
{
    if (SlotIndex < 0 || SlotIndex >= Slots.Num()) return;

    if (UQuickSlotSlotWidget* S = Slots[SlotIndex])
    {
        // 슬롯 하나만 갱신
        S->UpdateVisual(Item); // 래퍼(또는 UpdateVisualBP) 호출
    }
}
