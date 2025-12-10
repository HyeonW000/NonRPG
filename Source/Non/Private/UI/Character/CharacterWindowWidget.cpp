#include "UI/Character/CharacterWindowWidget.h"
#include "Inventory/InventoryComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "UI/Character/EquipmentSlotWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "AbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "Components/TextBlock.h"

void UCharacterWindowWidget::UpdateStats()
{
    APawn* OwningPawn = GetOwningPlayerPawn();
    if (!OwningPawn) return;

    UAbilitySystemComponent* ASC = OwningPawn->FindComponentByClass<UAbilitySystemComponent>();
    if (!ASC) return;
    
    // 1. Level
    if (Text_Level)
    {
        float Val = ASC->GetNumericAttribute(UNonAttributeSet::GetLevelAttribute());
        Text_Level->SetText(FText::AsNumber((int32)Val));
    }
    // 2. HP
    if (Text_HP)
    {
        float Cur = ASC->GetNumericAttribute(UNonAttributeSet::GetHPAttribute());
        float Max = ASC->GetNumericAttribute(UNonAttributeSet::GetMaxHPAttribute());
        Text_HP->SetText(FText::Format(FText::FromString(TEXT("{0} / {1}")), (int32)Cur, (int32)Max));
    }
    // 3. MP
    if (Text_MP)
    {
        float Cur = ASC->GetNumericAttribute(UNonAttributeSet::GetMPAttribute());
        float Max = ASC->GetNumericAttribute(UNonAttributeSet::GetMaxMPAttribute());
        Text_MP->SetText(FText::Format(FText::FromString(TEXT("{0} / {1}")), (int32)Cur, (int32)Max));
    }
    // 4. Attack
    if (Text_Atk)
    {
        float Val = ASC->GetNumericAttribute(UNonAttributeSet::GetAttackPowerAttribute());
        Text_Atk->SetText(FText::AsNumber((int32)Val));
    }
    // 5. Defense
    if (Text_Def)
    {
        float Val = ASC->GetNumericAttribute(UNonAttributeSet::GetDefenseAttribute());
        Text_Def->SetText(FText::AsNumber((int32)Val));
    }
    // 6. Critical Rate
    if (Text_CriticalRate)
    {
        float Val = ASC->GetNumericAttribute(UNonAttributeSet::GetCriticalRateAttribute());
        // For example: 30.5%
        Text_CriticalRate->SetText(FText::Format(FText::FromString(TEXT("{0}%")), FText::AsNumber((int32)Val))); // Using int for simplicity or float? User didn't specify. Assuming int for now based on previous pattern, but rate usually needs float. 
        // Actually, usually rate is 0-100 or 0-1. Assuming 0-100 based on '30' being typical value type. 
        // Let's use AsNumber with 1 decimal place? Or just integer. 
        // User asked "can I put critical rate/damage", implying basic stats.
        // Let's format as "15%"
        Text_CriticalRate->SetText(FText::Format(FText::FromString(TEXT("{0}%")), FText::AsNumber(Val, &FNumberFormattingOptions::DefaultNoGrouping())));
    }
    // 7. Critical Damage
    if (Text_CriticalDamage)
    {
        float Val = ASC->GetNumericAttribute(UNonAttributeSet::GetCriticalDamageAttribute());
        // For example: 150%
        Text_CriticalDamage->SetText(FText::Format(FText::FromString(TEXT("{0}%")), FText::AsNumber(Val, &FNumberFormattingOptions::DefaultNoGrouping())));
    }
}

void UCharacterWindowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UpdateStats(); // 초기화 시 스탯 갱신

    if (bInitOnce) return;

    // 1) 소유 폰/PC에서 EquipmentComponent 찾기
    UEquipmentComponent* Equip = nullptr;
    if (APawn* Pawn = GetOwningPlayerPawn())
    {
        Equip = Pawn->FindComponentByClass<UEquipmentComponent>();
    }
    if (!Equip && GetWorld())
    {
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                Equip = Pawn->FindComponentByClass<UEquipmentComponent>();
            }
        }
    }
    //  InitCharacterUI보다 NativeConstruct가 먼저 불리면 OwnerEquipment가 비어있을 수 있으므로 보강
    if (!OwnerEquipment && Equip)
    {
        OwnerEquipment = Equip;
    }

    if (!WidgetTree) return;

    // 2) 슬롯 위젯들 초기화 + 현재 장착 아이템으로 아이콘 설정
    TArray<UWidget*> AllWidgets;
    WidgetTree->GetAllWidgets(AllWidgets);

    int32 NumSlotsBound = 0;
    for (UWidget* W : AllWidgets)
    {
        if (auto* EquipSlotW = Cast<UEquipmentSlotWidget>(W))
        {
            if (OwnerEquipment) { EquipSlotW->OwnerEquipment = OwnerEquipment; }

            UInventoryItem* Curr = (OwnerEquipment)
                ? OwnerEquipment->GetEquippedItemBySlot(EquipSlotW->SlotType)
                : nullptr;

            EquipSlotW->ClearMirror();
            EquipSlotW->UpdateVisual(Curr);
            ++NumSlotsBound;
        }
    }
    bInitOnce = true;
}

void UCharacterWindowWidget::NativeDestruct()
{
    if (OwnerEquipment && bEquipDelegatesBound)
    {
        OwnerEquipment->OnEquipped.RemoveDynamic(this, &UCharacterWindowWidget::HandleEquipped);
        OwnerEquipment->OnUnequipped.RemoveDynamic(this, &UCharacterWindowWidget::HandleUnequipped);
        bEquipDelegatesBound = false;
    }
    Super::NativeDestruct();
}

static EEquipmentSlot GetPairedHand(EEquipmentSlot S)
{
    switch (S)
    {
    case EEquipmentSlot::WeaponMain: return EEquipmentSlot::WeaponSub;
    case EEquipmentSlot::WeaponSub:  return EEquipmentSlot::WeaponMain;
    default:                         return EEquipmentSlot::None;
    }
}

static UEquipmentSlotWidget* FindSlotWidgetByType(UWidgetTree* Tree, EEquipmentSlot Type)
{
    if (!Tree) return nullptr;
    UEquipmentSlotWidget* Found = nullptr;

    Tree->ForEachWidget([&](UWidget* W)
        {
            if (UEquipmentSlotWidget* S = Cast<UEquipmentSlotWidget>(W))
            {
                if (S->SlotType == Type)
                {
                    Found = S;
                }
            }
        });
    return Found;
}

static UEquipmentComponent* ResolveEquipment(UUserWidget* Widget)
{
    if (!Widget) return nullptr;
    if (AActor* OwningActor = Widget->GetOwningPlayerPawn())
    {
        return OwningActor->FindComponentByClass<UEquipmentComponent>();
    }
    if (AActor* OwningActor2 = Widget->GetOwningPlayer())
    {
        return OwningActor2->FindComponentByClass<UEquipmentComponent>();
    }
    return nullptr;
}

void UCharacterWindowWidget::HandleEquipped(EEquipmentSlot EquipSlot, UInventoryItem* Item)
{
    if (!WidgetTree) return;

    // 1) 해당 슬롯 위젯 갱신
    if (UEquipmentSlotWidget* ThisW = FindSlotWidgetByType(WidgetTree, EquipSlot))
    {
        ThisW->ClearMirror();
        ThisW->UpdateVisual(Item);
    }

    // 2) 반대 손 처리
    const EEquipmentSlot Pair = GetPairedHand(EquipSlot);
    if (Pair != EEquipmentSlot::None)
    {
        if (UEquipmentSlotWidget* PairW = FindSlotWidgetByType(WidgetTree, Pair))
        {
            const bool bTwoHanded = (Item && Item->IsEquipment() && Item->IsTwoHandedWeapon());
            if (bTwoHanded)
            {
                PairW->SetMirrorFrom(EquipSlot, Item);
            }
            else
            {
                PairW->ClearMirror();

                if (UEquipmentComponent* Eq = ResolveEquipment(this))
                {
                    PairW->UpdateVisual(Eq->GetEquippedItemBySlot(Pair));
                }
                else
                {
                    PairW->UpdateVisual(nullptr);
                }
            }
        }
    }
}

void UCharacterWindowWidget::HandleUnequipped(EEquipmentSlot EquipSlot)
{
    if (!WidgetTree) return;

    // 1) 해당 슬롯 위젯 비우기
    if (UEquipmentSlotWidget* ThisW = FindSlotWidgetByType(WidgetTree, EquipSlot))
    {
        ThisW->ClearMirror();
        ThisW->UpdateVisual(nullptr);
    }

    // 2) 반대 손 정리/재표시
    const EEquipmentSlot Pair = GetPairedHand(EquipSlot);
    if (Pair != EEquipmentSlot::None)
    {
        if (UEquipmentSlotWidget* PairW = FindSlotWidgetByType(WidgetTree, Pair))
        {
            PairW->ClearMirror();

            if (UEquipmentComponent* Eq = ResolveEquipment(this))
            {
                PairW->UpdateVisual(Eq->GetEquippedItemBySlot(Pair));
            }
            else
            {
                PairW->UpdateVisual(nullptr);
            }
        }
    }
}

void UCharacterWindowWidget::InitCharacterUI(UInventoryComponent* InInv, UEquipmentComponent* InEq)
{
    // 기존 바인딩 해제
    if (OwnerEquipment && bEquipDelegatesBound)
    {
        OwnerEquipment->OnEquipped.RemoveDynamic(this, &UCharacterWindowWidget::HandleEquipped);
        OwnerEquipment->OnUnequipped.RemoveDynamic(this, &UCharacterWindowWidget::HandleUnequipped);
        bEquipDelegatesBound = false;
    }

    OwnerInventory = InInv;
    OwnerEquipment = InEq;
    if (OwnerEquipment && !bEquipDelegatesBound)
    {
        OwnerEquipment->OnEquipped.AddUniqueDynamic(this, &UCharacterWindowWidget::HandleEquipped);
        OwnerEquipment->OnUnequipped.AddUniqueDynamic(this, &UCharacterWindowWidget::HandleUnequipped);
        bEquipDelegatesBound = true;
    }

    // 현재 장착상태 + 2H 미러까지 즉시 반영
    // 현재 장착상태 + 2H 미러까지 즉시 반영
    RefreshAllSlots();
    UpdateStats();
}


void UCharacterWindowWidget::PropagateEquipmentToSlots()
{
    if (!OwnerEquipment)
    {
        return;
    }

    if (!WidgetTree) return;

    TArray<UWidget*> All;
    WidgetTree->GetAllWidgets(All);

    for (UWidget* Child : All)
    {
        if (auto* EquipSlotWidget = Cast<UEquipmentSlotWidget>(Child))
        {
            EquipSlotWidget->SetOwnerEquipment(OwnerEquipment);

            EquipSlotWidget->RefreshFromComponent();   // 현재 장착 + 2H 고스트 한 번에
        }
    }
}

void UCharacterWindowWidget::RefreshAllSlots()
{
    PropagateEquipmentToSlots();
    ReapplyTwoHandMirrors();
}

void UCharacterWindowWidget::ReapplyTwoHandMirrors()
{
    if (!OwnerEquipment || !WidgetTree) return;

    // 무기 슬롯 미러 초기화
    if (auto* MainW = FindSlotWidgetByType(WidgetTree, EEquipmentSlot::WeaponMain)) MainW->ClearMirror();
    if (auto* SubW = FindSlotWidgetByType(WidgetTree, EEquipmentSlot::WeaponSub))  SubW->ClearMirror();

    auto MirrorIf2H = [&](EEquipmentSlot FromSlot)
        {
            if (UInventoryItem* It = OwnerEquipment->GetEquippedItemBySlot(FromSlot))
            {
                // 만약 반대 손에 실제 장비가 이미 있으면(방패 등) 미러는 스킵하는 게 자연스러움
                const EEquipmentSlot Pair = GetPairedHand(FromSlot);
                if (Pair != EEquipmentSlot::None && !OwnerEquipment->GetEquippedItemBySlot(Pair))
                {
                    if (It->IsEquipment() && It->IsTwoHandedWeapon())
                    {
                        if (auto* PairW = FindSlotWidgetByType(WidgetTree, Pair))
                            PairW->SetMirrorFrom(FromSlot, It);
                    }
                }
            }
        };

    MirrorIf2H(EEquipmentSlot::WeaponMain);
    MirrorIf2H(EEquipmentSlot::WeaponSub);
}