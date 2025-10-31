#include "UI/InventoryWidget.h"
#include "UI/InventorySlotWidget.h"
#include "Core/BPC_UIManager.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryItem.h"
#include "Equipment/EquipmentComponent.h"
#include "Inventory/ItemDragDropOperation.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Blueprint/WidgetTree.h"

void UInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 이미 InvRef가 세팅되어 있고 슬롯이 아직 없다면 초기화
    if (InvRef && Slots.Num() == 0)
    {
        BindDelegates();
        InitSlots();
        RefreshAll();
    }
}

void UInventoryWidget::NativeDestruct()
{
    UnbindDelegates();
    ClearSlots();
    Super::NativeDestruct();
}

void UInventoryWidget::SetInventory(UInventoryComponent* InInventory)
{
    if (InvRef == InInventory)
    {
        return;
    }

    UnbindDelegates();
    InvRef = InInventory;

    ClearSlots();

    if (InvRef)
    {
        BindDelegates();
        InitSlots();
        RefreshAll();
    }
}

void UInventoryWidget::BindDelegates()
{
    if (!InvRef) return;

    // InventoryComponent.h 에 선언된 블루프린트 어사이너블 델리게이트:
    // UPROPERTY(BlueprintAssignable) FOnInventorySlotUpdated OnSlotUpdated;
    // UPROPERTY(BlueprintAssignable) FOnInventoryRefreshed   OnInventoryRefreshed;
    InvRef->OnSlotUpdated.AddDynamic(this, &UInventoryWidget::HandleSlotUpdated);
    InvRef->OnInventoryRefreshed.AddDynamic(this, &UInventoryWidget::HandleInventoryRefreshed);
}

void UInventoryWidget::UnbindDelegates()
{
    if (!InvRef) return;
    InvRef->OnSlotUpdated.RemoveAll(this);
    InvRef->OnInventoryRefreshed.RemoveAll(this);
}

void UInventoryWidget::InitSlots()
{
    if (!Grid)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryWidget] Grid is null (BindWidget name mismatch?)."));
        return;
    }
    if (!SlotWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryWidget] SlotWidgetClass is not set."));
        return;
    }

    ClearSlots();

    const int32 Count = InvRef ? InvRef->GetSlotCount() : 0;
    if (Count <= 0) return;

    Slots.SetNum(Count);

    for (int32 Index = 0; Index < Count; ++Index)
    {
        UInventorySlotWidget* SlotWidget = CreateWidget<UInventorySlotWidget>(this, SlotWidgetClass);
        if (!SlotWidget) continue;

        // 여기서 한 줄로 초기화 + 델리게이트 바인딩까지 처리
        SlotWidget->InitSlot(InvRef, Index);

        const int32 Row = Columns > 0 ? Index / Columns : 0;
        const int32 Col = Columns > 0 ? Index % Columns : Index;

        if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(SlotWidget, Row, Col))
        {
            // 필요 시 정렬/패딩 등 추가 설정
            // GridSlot->SetHorizontalAlignment(HAlign_Fill);
            // GridSlot->SetVerticalAlignment(VAlign_Fill);
        }

        // 여기서는 'SlotWidget' 을 저장해야 함
        Slots[Index] = SlotWidget;
    }

    InvalidateLayoutAndVolatility();
    ForceLayoutPrepass();
}

void UInventoryWidget::InitInventoryUI(UInventoryComponent* InInventory, UEquipmentComponent* InEquipment)
{
    OwnerInventory = InInventory;
    OwnerEquipment = InEquipment;

    // 핵심: SetInventory가 내부에서 델리게이트 바인딩 → 슬롯 생성(InitSlots) → 전체 갱신까지 처리
    SetInventory(InInventory);

    UE_LOG(LogTemp, Warning, TEXT("[InvUI] InitInventoryUI -> Inv=%s, Eq=%s, BuiltSlots=%d"),
        *GetNameSafe(InInventory), *GetNameSafe(InEquipment), Slots.Num());
}


void UInventoryWidget::ClearSlots()
{
    if (Grid)
    {
        Grid->ClearChildren();
    }
    Slots.Empty();
}

void UInventoryWidget::RefreshAll()
{
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (UInventorySlotWidget* S = Slots[i])
        {
            // 슬롯 위젯이 가진 갱신 루틴(RefreshFromInventory)을 호출
            S->RefreshFromInventory();
        }
    }
}

void UInventoryWidget::HandleSlotUpdated(int32 Index, UInventoryItem* /*Item*/)
{
    if (!Slots.IsValidIndex(Index)) return;
    if (UInventorySlotWidget* S = Slots[Index])
    {
        S->RefreshFromInventory();
    }
}

void UInventoryWidget::HandleInventoryRefreshed()
{
    RefreshAll();
}

bool UInventoryWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    // 장비에서 끌고온 드래그면 창 어딘가에 드롭 가능
    if (const UItemDragDropOperation* Op = Cast<UItemDragDropOperation>(InOperation))
    {
        if (Op->bFromEquipment)
        {
            return true; // 배경 어디든 OK
        }
    }
    return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
}

bool UInventoryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    const UItemDragDropOperation* Op = Cast<UItemDragDropOperation>(InOperation);
    if (!Op || !OwnerEquipment)
    {
        return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
    }

    // 장비슬롯에서 끌고온 경우 → 장착 해제해서 인벤토리로
    if (Op->bFromEquipment && Op->FromEquipSlot != EEquipmentSlot::None)
    {
        int32 OutIndex = INDEX_NONE;
        const bool bOk = OwnerEquipment->UnequipToInventory(Op->FromEquipSlot, OutIndex);
        if (bOk)
        {
            RefreshAll(); // 인벤토리 슬롯들 새로고침

            // ★ 캐릭터창 장비 슬롯도 즉시 갱신
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                // UIManager 찾아서 호출
                UBPC_UIManager* UI = nullptr;

                // OwnerEquipment 소유자 → 컴포넌트에서 먼저 찾아보고
                if (AActor* OwnerActor = OwnerEquipment->GetOwner())
                {
                    UI = OwnerActor->FindComponentByClass<UBPC_UIManager>();
                }
                // 못 찾으면 Pawn/Controller 경유
                if (!UI)
                {
                    if (APawn* Pawn = PC->GetPawn())
                        UI = Pawn->FindComponentByClass<UBPC_UIManager>();
                    if (!UI)
                        UI = PC->FindComponentByClass<UBPC_UIManager>();
                }

                if (UI)
                {
                    UI->RefreshCharacterEquipmentUI(); // 내부에서 CharacterWindow->RefreshAllSlots()
                }
            }

            return true;
        }
        return false;
    }

    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}