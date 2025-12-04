#include "UI/EquipmentSlotWidget.h"
#include "Inventory/ItemDragDropOperation.h"
#include "Inventory/InventoryItem.h"
#include "Inventory/ItemEnums.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Data/ItemStructs.h"
#include "Equipment/EquipmentComponent.h"
#include "UI/CharacterWindowWidget.h"
#include "InputCoreTypes.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogEquipSlotW, Log, All);

void UEquipmentSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BindEquipmentDelegates();

    // 시작 시 현재 장착 상태 반영
    if (OwnerEquipment)
    {
        RefreshFromComponent();
    }
}

void UEquipmentSlotWidget::NativeDestruct()
{
    UnbindEquipmentDelegates();
    Super::NativeDestruct();
}

void UEquipmentSlotWidget::BindEquipmentDelegates()
{
    if (!OwnerEquipment) return;
    // 중복 바인딩 방지
    OwnerEquipment->OnEquipped.RemoveAll(this);
    OwnerEquipment->OnUnequipped.RemoveAll(this);

    OwnerEquipment->OnEquipped.AddDynamic(this, &ThisClass::HandleEquipped);
    OwnerEquipment->OnUnequipped.AddDynamic(this, &ThisClass::HandleUnequipped);
}

void UEquipmentSlotWidget::UnbindEquipmentDelegates()
{
    if (!OwnerEquipment) return;
    OwnerEquipment->OnEquipped.RemoveAll(this);
    OwnerEquipment->OnUnequipped.RemoveAll(this);
}

void UEquipmentSlotWidget::HandleEquipped(EEquipmentSlot InEquipSlot, UInventoryItem* Item)
{
    if (InEquipSlot == SlotType || InEquipSlot == EEquipmentSlot::WeaponMain || InEquipSlot == EEquipmentSlot::WeaponSub)
    {
        RefreshFromComponent();
    }
}

void UEquipmentSlotWidget::HandleUnequipped(EEquipmentSlot InEquipSlot)
{
    if (InEquipSlot == SlotType || InEquipSlot == EEquipmentSlot::WeaponMain || InEquipSlot == EEquipmentSlot::WeaponSub)
    {
        RefreshFromComponent();
    }
}

// 슬롯-아이템 호환 규칙
static bool IsEquipmentForSlot(const UInventoryItem* Item, EEquipmentSlot SlotType)
{
    if (!Item || !Item->IsEquipment()) return false;

    // UInventoryItem 내부 캐시 행에서 EquipSlot 얻는다고 가정
    const EEquipmentSlot ItemSlot = Item->GetEquipSlot(); // 네가 만든 접근자
    if (ItemSlot == EEquipmentSlot::None) return false;

    // 무기는 무기 슬롯만, 머리는 머리 슬롯만 …
    return (ItemSlot == SlotType);
}

// ==== = 미러(고스트) 지원 ==== =

void UEquipmentSlotWidget::RefreshFromComponent()
{
    if (!OwnerEquipment)
    {
        UpdateVisual(nullptr);
        ApplyGhost(false);      // ← 고스트 해제
        return;
    }

    UInventoryItem* Item = OwnerEquipment->GetEquippedItemBySlot(SlotType);
    bool bGhost = false;

    if (!Item && SlotType == EEquipmentSlot::WeaponSub)
    {
        if (OwnerEquipment->IsMainHandOccupyingBothHands())
        {
            Item = OwnerEquipment->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain);
            bGhost = (Item != nullptr);
        }
    }

    UpdateVisual(Item);         // 아이콘(있으면 세팅, 없으면 비움)
    ApplyGhost(bGhost);         // ← 여기만 호출
}

void UEquipmentSlotWidget::SetMirrorFrom(EEquipmentSlot From, UInventoryItem* Item)
{
    bIsMirror = true;
    bMirrorGhost = true;       // 고스트 상태 ON
    MirrorFrom = From;

    ApplyGhost(true);          // 고스트 오버레이 표시

    if (IconImage)
    {
        if (Item && Item->GetIcon())
        {
            IconImage->SetBrushFromTexture(Item->GetIcon());
            IconImage->SetColorAndOpacity(FLinearColor(1, 1, 1, 0.35f));
            IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            IconImage->SetBrush(FSlateBrush());
            IconImage->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

// 미러 해제 시
void UEquipmentSlotWidget::ClearMirror()
{
    bIsMirror = false;
    bMirrorGhost = false;      //  고스트 상태 OFF
    MirrorFrom = EEquipmentSlot::None;

    ApplyGhost(false);         //  고스트 오버레이 숨김

    if (IconImage)
    {
        IconImage->SetColorAndOpacity(FLinearColor::White);
    }
}

void UEquipmentSlotWidget::UpdateVisual(UInventoryItem* Item)
{
    // 1) 매번 하드 리셋: 고스트/미러/하이라이트/아이콘 틴트 모두 초기화
    bIsMirror = false;
    bMirrorGhost = false;
    MirrorFrom = EEquipmentSlot::None;
    ApplyGhost(false); // HighlightBorder 접기
    if (HighlightBorder) { HighlightBorder->SetVisibility(ESlateVisibility::Collapsed); }

    if (!IconImage) return;

    if (Item && Item->GetIcon())
    {
        IconImage->SetBrushFromTexture(Item->GetIcon(), /*bMatchSize*/ true);
        IconImage->SetBrushTintColor(FSlateColor(FLinearColor::White)); //  추가: Brush Tint도 화이트로
        IconImage->SetColorAndOpacity(FLinearColor::White);             //  유지: ColorAndOpacity도 화이트
        IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        IconImage->SetBrush(FSlateBrush());
        IconImage->SetVisibility(ESlateVisibility::Collapsed);
    }
}


bool UEquipmentSlotWidget::CanAcceptItem(const UInventoryItem* Item) const
{
    if (!Item) return false;

    // 미러(고스트) 슬롯이면 입력/드랍 자체를 막음
    if (bIsMirror) return false;

    // 슬롯 타입과 아이템 타입 매칭
    if (!IsEquipmentForSlot(Item, SlotType)) return false;

    // 양손무기 제한: Sub 슬롯에는 금지
    if (Item->IsTwoHandedWeapon() && SlotType == EEquipmentSlot::WeaponSub)
        return false;

    // (원하면 반대로: 메인슬롯에는 언제나 OK)
    return true;
}

void UEquipmentSlotWidget::SetHighlight(bool bShow, bool bAllowed)
{
    if (!HighlightBorder && !IconImage) return;

    // 미러(고스트) 슬롯에서 OFF가 오면: 고스트 유지(보더 색/가시성만 고스트로)
    if ((bIsMirror || bMirrorGhost) && !bShow)
    {
        ApplyGhost(true);  // 고스트로 복귀
        return;
    }

    if (bShow)
    {
        const FLinearColor C = bAllowed
            ? FLinearColor(1.f, 1.f, 1.f, 0.28f)    // 허용(밝은 흰색)
            : FLinearColor(1.f, 0.2f, 0.2f, 0.35f); // 불가(옅은 빨강)

        if (HighlightBorder)
        {
            HighlightBorder->SetBrushColor(C);
            HighlightBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            // 미러 슬롯은 아이콘 틴트 건드리지 않음(고스트 유지)
            if (!(bIsMirror || bMirrorGhost) && IconImage)
            {
                IconImage->SetColorAndOpacity(C);
            }
        }
    }
    else
    {
        if (HighlightBorder)
        {
            HighlightBorder->SetVisibility(ESlateVisibility::Collapsed);
        }
        // 미러가 아닐 때만 원색 복귀
        if (IconImage && !(bIsMirror || bMirrorGhost))
        {
            IconImage->SetColorAndOpacity(FLinearColor::White);
        }
    }
}

bool UEquipmentSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (bMirrorGhost)
    {
        ApplyGhost(true); // 미러는 고스트 유지
        Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
        return false;
    }

    // 메인 슬롯: 드롭 시작 시점에 일단 OFF (잔상 제거)
    SetHighlight(false, false);

    if (!OwnerEquipment)
    {
        return false;
    }

    UItemDragDropOperation* DragOp = Cast<UItemDragDropOperation>(InOperation);
    if (!DragOp || DragOp->Item == nullptr)
    {
        return false;
    }

    UInventoryItem* DragItem = DragOp->Item;
    if (!CanAcceptItem(DragItem))
    {
        // 받아줄 수 없으면 즉시 OFF 보장
        SetHighlight(false, false);
        return false;
    }

    if (!DragOp->SourceInventory || DragOp->SourceIndex == INDEX_NONE)
    {
        SetHighlight(false, false);
        return false;
    }

    const bool bEquipped = OwnerEquipment->EquipFromInventory(DragOp->SourceInventory, DragOp->SourceIndex, SlotType);
    if (!bEquipped)
    {
        SetHighlight(false, false);
        return false;
    }

    UpdateVisual(DragItem);

    if (UCharacterWindowWidget* OwnerWin = GetTypedOuter<UCharacterWindowWidget>())
    {
        OwnerWin->RefreshAllSlots();
    }

    return true;
}

void UEquipmentSlotWidget::NativeOnDragEnter(const FGeometry& Geo, const FDragDropEvent& Ev, UDragDropOperation* Op)
{
    if (bMirrorGhost)
    {
        // 미러: 올라오면 빨강 경고
        SetHighlight(true, /*allowed*/ false);
        Super::NativeOnDragEnter(Geo, Ev, Op);
        return;
    }

    if (const UItemDragDropOperation* ItemOp = Cast<UItemDragDropOperation>(Op))
    {
        const bool bAllow = CanAcceptItem(const_cast<UInventoryItem*>(ItemOp->Item.Get()));
        SetHighlight(true, bAllow);
    }
    else
    {
        SetHighlight(false, false);
    }

    Super::NativeOnDragEnter(Geo, Ev, Op);
}

bool UEquipmentSlotWidget::NativeOnDragOver(const FGeometry& Geo, const FDragDropEvent& Ev, UDragDropOperation* Op)
{
    if (bMirrorGhost)
    {
        // 미러: 드롭 불가 유지
        return false;
    }

    const UItemDragDropOperation* D = Cast<UItemDragDropOperation>(Op);
    const UInventoryItem* It = D ? D->Item.Get() : nullptr;
    const bool bAllowed = CanAcceptItem(const_cast<UInventoryItem*>(It));
    SetHighlight(true, bAllowed);
    return bAllowed;
}

void UEquipmentSlotWidget::NativeOnDragLeave(const FDragDropEvent& Ev, UDragDropOperation* Op)
{
    if (bMirrorGhost)
    {
        // 미러: 떠나면 고스트로 복귀
        SetHighlight(false, false); // 내부 가드가 고스트로 되돌림
        Super::NativeOnDragLeave(Ev, Op);
        return;
    }

    // 메인 슬롯: 하이라이트 제거
    SetHighlight(false, false);
    Super::NativeOnDragLeave(Ev, Op);
}

void UEquipmentSlotWidget::NativeOnDragCancelled(const FDragDropEvent& Ev, UDragDropOperation* Op)
{
    if (bMirrorGhost)
    {
        // 미러: 항상 고스트로
        ApplyGhost(true);
        Super::NativeOnDragCancelled(Ev, Op);
        return;
    }

    // 메인 슬롯: 어떤 경우든 하이라이트 제거
    SetHighlight(false, false);
    Super::NativeOnDragCancelled(Ev, Op);
}

void UEquipmentSlotWidget::NativeOnDragDetected(
    const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    OutOperation = nullptr;

    if (!OwnerEquipment) return;
    if (bIsMirror || bMirrorGhost) return;

    UInventoryItem* Equipped = OwnerEquipment->GetEquippedItemBySlot(SlotType);
    if (!Equipped) return;

    UItemDragDropOperation* Op =
        Cast<UItemDragDropOperation>(UWidgetBlueprintLibrary::CreateDragDropOperation(UItemDragDropOperation::StaticClass()));
    if (!Op) return;

    Op->bFromEquipment = true;
    Op->FromEquipSlot = SlotType;
    Op->Item = Equipped;
    Op->SourceInventory = nullptr;
    Op->SourceIndex = INDEX_NONE;

    const float IconSize = 64.f;
    UWidget* Visual = nullptr;

    if (DragVisualClass)
    {
        if (UUserWidget* VW = CreateWidget<UUserWidget>(GetWorld(), DragVisualClass))
        {
            if (UImage* Img = Cast<UImage>(VW->GetWidgetFromName(TEXT("IconImage"))))
            {
                FSlateBrush Brush = Img->GetBrush();
                Brush.SetResourceObject(Equipped->GetIcon());
                Brush.ImageSize = FVector2D(IconSize, IconSize);
                Brush.DrawAs = ESlateBrushDrawType::Image;
                Img->SetBrush(Brush);
                Img->SetVisibility(ESlateVisibility::HitTestInvisible);
            }
            // 루트가 UserWidget이면 원하는 픽셀 크기 지정 가능
            VW->SetDesiredSizeInViewport(FVector2D(IconSize, IconSize));
            Visual = VW;
        }
    }

    

    Op->DefaultDragVisual = Visual;
    Op->Pivot = EDragPivot::MouseDown;
    Op->Offset = FVector2D::ZeroVector;

    OutOperation = Op;
}

void UEquipmentSlotWidget::ApplyGhost(bool bOn)
{
    if (!HighlightBorder) return;

    if (bOn)
    {
        // 고스트(옅은 흰색)
        HighlightBorder->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.12f));
        HighlightBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    else
    {
        HighlightBorder->SetVisibility(ESlateVisibility::Collapsed);
    }
}

FReply UEquipmentSlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    return FReply::Unhandled(); // 여기선 일단 넘김 (테스트용)
}

FReply UEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // DetectDragIfPressed는 FEventReply를 반환 → .NativeReply로 FReply 꺼내서 반환
        FEventReply Ev = UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton);
        return Ev.NativeReply;
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UEquipmentSlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
    }

    if (!OwnerEquipment)
    {
        return FReply::Handled();
    }

    // 미러 슬롯은 입력 무시
    if (bIsMirror || bMirrorGhost)   //  여기 bMirrorGhost 추가
    {
        return FReply::Handled();
    }

    // 현재 슬롯에 장착된 아이템이 있으면 → 해제(인벤토리로)
    if (UInventoryItem* CurrentEquipped = OwnerEquipment->GetEquippedItemBySlot(SlotType))
    {
        int32 OutInventoryIndex = INDEX_NONE; //  시그니처: (SlotType, OutIndex)
        const bool bUnequipped = OwnerEquipment->UnequipToInventory(SlotType, OutInventoryIndex);
        if (bUnequipped)
        {
            UpdateVisual(nullptr);

            if (UCharacterWindowWidget* OwnerWin = GetTypedOuter<UCharacterWindowWidget>())
            {
                OwnerWin->RefreshAllSlots(); // 슬롯/미러 동시 재적용
            }
        }
        else
        {
        }
        return FReply::Handled();
    }

    // 빈 슬롯: 프로젝트 규칙에 맞는 "더블클릭 장착" 루틴이 있으면 여기에 붙이면 됨.
    return FReply::Unhandled();
}