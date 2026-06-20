#include "UI/Inventory/InventoryWidget.h"
#include "UI/Inventory/InventorySlotWidget.h"
#include "Core/NonUIManagerComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryItem.h"
#include "Equipment/EquipmentComponent.h"
#include "Inventory/ItemDragDropOperation.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

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
    // [New Fix] 인벤토리 가방 창이 화면에서 닫히거나 숨겨질 때, 미확인된 빨간 점을 일괄적으로 백그라운드 청소 처리하는 기능을 비활성화했습니다.
    // (이유: 창이 닫히거나 재생성되는 타이밍에 아직 확인되지 않은 새 아이템의 알람까지 모두 지워지는 현상 방지)
    /*
    if (InvRef)
    {
        InvRef->ClearAllNewItemFlags();
    }
    else if (OwnerInventory)
    {
        OwnerInventory->ClearAllNewItemFlags();
    }
    */

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
    InvRef->OnGoldChanged.AddDynamic(this, &UInventoryWidget::HandleGoldChanged);
}

void UInventoryWidget::UnbindDelegates()
{
    if (!InvRef) return;
    InvRef->OnSlotUpdated.RemoveAll(this);
    InvRef->OnInventoryRefreshed.RemoveAll(this);
    InvRef->OnGoldChanged.RemoveAll(this);
}

void UInventoryWidget::InitSlots()
{
    if (!Grid)
    {
        return;
    }
    if (!SlotWidgetClass)
    {
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
    UpdateGoldText();
    ApplyFilter();
}

void UInventoryWidget::HandleSlotUpdated(int32 Index, UInventoryItem* /*Item*/)
{
    if (!Slots.IsValidIndex(Index)) return;
    if (UInventorySlotWidget* S = Slots[Index])
    {
        S->RefreshFromInventory();
    }
    ApplyFilter();
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
                UNonUIManagerComponent* UI = nullptr;

                // OwnerEquipment 소유자 → 컴포넌트에서 먼저 찾아보고
                if (AActor* OwnerActor = OwnerEquipment->GetOwner())
                {
                    UI = OwnerActor->FindComponentByClass<UNonUIManagerComponent>();
                }
                // 못 찾으면 Pawn/Controller 경유
                if (!UI)
                {
                    if (APawn* Pawn = PC->GetPawn())
                        UI = Pawn->FindComponentByClass<UNonUIManagerComponent>();
                    if (!UI)
                        UI = PC->FindComponentByClass<UNonUIManagerComponent>();
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

void UInventoryWidget::HandleGoldChanged(int32 NewGold)
{
    UpdateGoldText();
}

void UInventoryWidget::UpdateGoldText()
{
    if (!GoldText) return;
    if (!InvRef)
    {
        GoldText->SetText(FText::FromString(TEXT("0")));
        return;
    }

    // FText::AsNumber는 3자리마다 자동으로 쉽표(,)를 넣어 정교하게 포맷팅해 줍니다.
    GoldText->SetText(FText::AsNumber(InvRef->GetGold()));
}

void UInventoryWidget::ToggleFilter(EItemType FilterType)
{
    if (bIsFilterActive && ActiveFilterType == FilterType)
    {
        // 동일한 필터를 또 누르면 필터링 비활성화 (전체보기 복귀)
        bIsFilterActive = false;
    }
    else
    {
        // 필터 활성화 또는 새로운 필터로 교체
        bIsFilterActive = true;
        ActiveFilterType = FilterType;
    }

    // 전 슬롯 필터 비주얼 적용
    ApplyFilter();
}

void UInventoryWidget::ApplyFilter()
{
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        UInventorySlotWidget* SlotWidget = Slots[i];
        if (!SlotWidget) continue;

        if (!bIsFilterActive)
        {
            // 필터가 해제되어 있으면 모든 슬롯을 환하고 상호작용 가능하게 원상 복귀
            SlotWidget->SetFilterState(false);
        }
        else
        {
            // 필터가 켜져 있으면
            UInventoryItem* SlotItem = SlotWidget->Item;
            if (SlotItem)
            {
                // 아이템 대분류가 활성 필터와 일치하면 활성, 다르면 필터아웃(회색조/반투명) 처리!
                const bool bMatch = (SlotItem->CachedRow.ItemType == ActiveFilterType);
                SlotWidget->SetFilterState(!bMatch);
            }
            else
            {
                // 빈 슬롯은 필터 조건에 무조건 맞지 않으므로 톤다운 처리!
                SlotWidget->SetFilterState(true);
            }
        }
    }
}