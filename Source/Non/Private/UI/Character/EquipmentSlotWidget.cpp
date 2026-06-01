#include "UI/Character/EquipmentSlotWidget.h"
#include "UI/Inventory/ItemToolTipWidget.h" // ◀ [New] 장착 슬롯 호버링 툴팁 생성용 헤더 포함!
#include "Engine/Engine.h"                 // ◀ [New] 툴팁 작동 유무를 화면에 노란 글씨로 즉시 증명해줄 엔진 헤더 포함!
#include "Inventory/ItemDragDropOperation.h"
#include "Inventory/InventoryItem.h"
#include "Inventory/ItemEnums.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Data/ItemStructs.h"
#include "Equipment/EquipmentComponent.h"
#include "UI/Character/CharacterWindowWidget.h"
#include "InputCoreTypes.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/UIViewportUtils.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY_STATIC(LogEquipSlotW, Log, All);

void UEquipmentSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(false); // 포커스 방지
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

    // [New] 양손 무기 점유로 인한 서브 무기 슬롯 비활성화(대기 고스트) 상태 감지
    bool bIsWeaponSubGhost = false;
    if (SlotType == EEquipmentSlot::WeaponSub && OwnerEquipment != nullptr)
    {
        if (OwnerEquipment->IsMainHandOccupyingBothHands() && OwnerEquipment->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain) != nullptr)
        {
            bIsWeaponSubGhost = true;
        }
    }

    // 2) 장착 아이템 유무에 따른 테두리(BorderIconFrame) 및 뒷배경(BorderBackground) 실시간 등급 컬러 렌더링
    // (대기 고스트 상태일 때는 쨍한 등급 색상을 끄고 미장착 상태처럼 어둡게 표기합니다)
    if (Item && !bIsWeaponSubGhost)
    {
        FLinearColor RarityColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f); // 기본 회색 (Common)
        
        // 디자이너가 에디터에서 설정한 RarityColors가 있으면 그것을 최우선적으로 상속받아 칠합니다
        if (RarityColors.Contains(Item->CachedRow.Rarity))
        {
            RarityColor = RarityColors[Item->CachedRow.Rarity];
        }
        else
        {
            switch (Item->CachedRow.Rarity)
            {
                case EItemRarity::Uncommon:  RarityColor = FLinearColor(0.12f, 0.77f, 0.12f, 1.0f); break; // 고급 (연초록)
                case EItemRarity::Rare:      RarityColor = FLinearColor(0.0f, 0.47f, 0.95f, 1.0f);  break; // 희귀 (파랑)
                case EItemRarity::Epic:      RarityColor = FLinearColor(0.63f, 0.13f, 0.94f, 1.0f); break; // 영웅 (보라)
                case EItemRarity::Legendary: RarityColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);    break; // 전설 (황금빛 주황)
                case EItemRarity::Mythic:    RarityColor = FLinearColor(0.85f, 0.08f, 0.23f, 1.0f); break; // 신화 (적색)
                default: break;
            }
        }

        if (BorderIconFrame)
        {
            BorderIconFrame->SetBrushColor(RarityColor);
            BorderIconFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        if (BorderBackground)
        {
            FLinearColor BgColor = RarityColor;
            BgColor.A = BorderBackground->GetBrushColor().A; // 디자이너가 설정한 알파값 보존
            BorderBackground->SetBrushColor(BgColor);
            BorderBackground->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
    }
    else
    {
        // 미장착(빈 슬롯)이거나 대기 고스트 상태일 때는 보더들을 얌전한 기본 투명 어둠 비주얼로 되돌립니다.
        if (BorderIconFrame)
        {
            // 빈 슬롯은 테두리를 투명한 짙은 회색선으로 은은하게 표기합니다.
            BorderIconFrame->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
        }
        if (BorderBackground)
        {
            FLinearColor TargetColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
            TargetColor.A = BorderBackground->GetBrushColor().A; // 디자이너가 설정한 알파값 보존
            BorderBackground->SetBrushColor(TargetColor);
        }
    }

    if (!IconImage) return;

    if (Item && Item->GetIcon())
    {
        IconImage->SetBrushFromTexture(Item->GetIcon(), /*bMatchSize*/ true);
        IconImage->SetBrushTintColor(FSlateColor(FLinearColor::White)); //  추가: Brush Tint도 화이트로
        
        // 대기 고스트 상태일 때는 아이콘의 불투명도를 30% 반투명으로 낮추어 흐릿한 잔상으로 연출합니다
        if (bIsWeaponSubGhost)
        {
            IconImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.3f)); // 30% 은은한 실루엣 잔상
        }
        else
        {
            IconImage->SetColorAndOpacity(FLinearColor::White);             //  유지: ColorAndOpacity도 화이트
        }
        IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        // 빈 슬롯일 때 지정된 가이드 아이콘이 있다면 25% 반투명 은은한 가이드로 표출
        if (EmptySlotIcon)
        {
            IconImage->SetBrushFromTexture(EmptySlotIcon, /*bMatchSize*/ true);
            IconImage->SetBrushTintColor(FSlateColor(FLinearColor::White));
            IconImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.25f)); // 25% 반투명 아우라 가이드
            IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            IconImage->SetBrush(FSlateBrush());
            IconImage->SetVisibility(ESlateVisibility::Collapsed);
        }
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
            // 1. 아이콘 이미지 리소스 주입
            if (UImage* Img = Cast<UImage>(VW->GetWidgetFromName(TEXT("IconImage"))))
            {
                FSlateBrush Brush = Img->GetBrush();
                Brush.SetResourceObject(Equipped->GetIcon());
                Brush.ImageSize = FVector2D(IconSize, IconSize);
                Brush.DrawAs = ESlateBrushDrawType::Image;
                Img->SetBrush(Brush);
                Img->SetVisibility(ESlateVisibility::HitTestInvisible);
            }

            // 2. [New UI UX] 드래그 비주얼 위젯에도 등급 테두리(BorderIconFrame)가 존재하면 실시간 등급 전용 컬러 포인트 주입
            if (UBorder* Frame = Cast<UBorder>(VW->GetWidgetFromName(TEXT("BorderIconFrame"))))
            {
                FLinearColor RarityColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f); // 기본 회색 (Common)
                if (RarityColors.Contains(Equipped->CachedRow.Rarity))
                {
                    RarityColor = RarityColors[Equipped->CachedRow.Rarity];
                }
                else
                {
                    switch (Equipped->CachedRow.Rarity)
                    {
                        case EItemRarity::Uncommon:  RarityColor = FLinearColor(0.12f, 0.77f, 0.12f, 1.0f); break; // 고급 (연초록)
                        case EItemRarity::Rare:      RarityColor = FLinearColor(0.0f, 0.47f, 0.95f, 1.0f);  break; // 희귀 (파랑)
                        case EItemRarity::Epic:      RarityColor = FLinearColor(0.63f, 0.13f, 0.94f, 1.0f); break; // 영웅 (보라)
                        case EItemRarity::Legendary: RarityColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);    break; // 전설 (황금빛 주황)
                        case EItemRarity::Mythic:    RarityColor = FLinearColor(0.85f, 0.08f, 0.23f, 1.0f); break; // 신화 (적색)
                        default: break;
                    }
                }
                Frame->SetBrushColor(RarityColor);

                // 2.5. [New UI UX] 드래그 뒷배경 보더(BorderBackground)가 존재하면 등급 전용 컬러의 은은한 반투명 아우라 실시간 연동 주입
                if (UBorder* Bg = Cast<UBorder>(VW->GetWidgetFromName(TEXT("BorderBackground"))))
                {
                    FLinearColor BgColor = RarityColor;
                    BgColor.A = Bg->GetBrushColor().A; // 디자이너가 설정한 알파값 보존
                    Bg->SetBrushColor(BgColor);
                }
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
        // C++는 색상에 일절 간섭하지 않고, 디렉터님이 UMG에서 아름답게 디자인한 오버레이 보더를 켜주기만 합니다!
        HighlightBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    else
    {
        HighlightBorder->SetVisibility(ESlateVisibility::Collapsed);
    }
}

FReply UEquipmentSlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
         // 1. Input Mode Ensure
        if (APlayerController* PC = GetOwningPlayer())
        {
            FInputModeGameAndUI Mode;
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Mode.SetHideCursorDuringCapture(false);
            Mode.SetWidgetToFocus(nullptr);
            PC->SetInputMode(Mode);
        }

        // 2. Detect Drag
        FEventReply Ev = UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton);
        FReply Reply = Ev.NativeReply;

        // 3. Force Focus back to Viewport
        if (TSharedPtr<SViewport> VP = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
        {
            Reply = Reply.SetUserFocus(StaticCastSharedRef<SWidget>(VP.ToSharedRef()), EFocusCause::SetDirectly);
            FSlateApplication::Get().SetKeyboardFocus(StaticCastSharedPtr<SWidget>(VP), EFocusCause::SetDirectly);
        }
        return Reply;
    }
    return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Deprecated: Moved to Preview
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
        // Viewport Focus Restore even if handled early
        if (TSharedPtr<SViewport> VP = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
        {
            FSlateApplication::Get().SetUserFocus(FSlateApplication::Get().GetUserIndexForKeyboard(), StaticCastSharedRef<SWidget>(VP.ToSharedRef()), EFocusCause::SetDirectly);
            FSlateApplication::Get().SetKeyboardFocus(StaticCastSharedPtr<SWidget>(VP), EFocusCause::SetDirectly);
        }
        return FReply::Handled();
    }

    // Force Focus back to Viewport (Crucial for WASD continuity)
    if (TSharedPtr<SViewport> VP = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
    {
        FSlateApplication::Get().SetUserFocus(FSlateApplication::Get().GetUserIndexForKeyboard(), StaticCastSharedRef<SWidget>(VP.ToSharedRef()), EFocusCause::SetDirectly);
        FSlateApplication::Get().SetKeyboardFocus(StaticCastSharedPtr<SWidget>(VP), EFocusCause::SetDirectly);
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

UWidget* UEquipmentSlotWidget::GetCustomToolTipWidget()
{
    if (!OwnerEquipment) return nullptr;

    UInventoryItem* EquippedItem = OwnerEquipment->GetEquippedItemBySlot(SlotType);
    if (!EquippedItem) return nullptr;

    if (!ToolTipWidgetClass) return nullptr;

    // ◀ [New UI UX] 템플릿 캐스팅 엄격성으로 인한 생성 실패를 완전히 회피하기 위해,
    // 생성은 최상위 UUserWidget으로 부드럽게 완료한 후 내부에서 UItemToolTipWidget 캐스팅을 시도하도록 우회 설계합니다!
    UUserWidget* CreatedWidget = CreateWidget<UUserWidget>(this, ToolTipWidgetClass);
    if (CreatedWidget)
    {
        if (UItemToolTipWidget* ToolTip = Cast<UItemToolTipWidget>(CreatedWidget))
        {
            ToolTip->bIsComparePanel = true;
            ToolTip->SetItemData(EquippedItem);
        }
        return CreatedWidget;
    }
    return nullptr;
}

void UEquipmentSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

    // 1. 기존에 임시 보관 중인 툴팁 위젯이 있다면 메모리를 정돈합니다.
    if (ActiveToolTipInstance)
    {
        ActiveToolTipInstance = nullptr;
    }

    // 2. 동적 수동 툴팁 위젯 생성 구문 기동
    UWidget* CustomToolTip = GetCustomToolTipWidget();
    if (CustomToolTip)
    {
        ActiveToolTipInstance = Cast<UUserWidget>(CustomToolTip);
        if (ActiveToolTipInstance)
        {
            // 3. [New UI UX] UMG의 델리게이트 꼬임 현상을 완전히 우회하여, 
            // 마우스 진입 시점에 툴팁 위젯을 본체 슬롯에 수동으로 즉각 강제 세팅 꼽아 줍니다!
            SetToolTip(ActiveToolTipInstance);
        }
    }
}

void UEquipmentSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);

    // 4. 마우스가 슬롯 밖으로 탈출하면 강제 설정해 주었던 툴팁을 비워서 안전하게 소멸시킵니다.
    SetToolTip(nullptr);
    ActiveToolTipInstance = nullptr;
}