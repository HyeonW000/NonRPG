#include "Inventory/ItemUseLibrary.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryItem.h"
#include "Equipment/EquipmentComponent.h"
#include "AbilitySystemComponent.h"
#include "Core/BPC_UIManager.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

static UBPC_UIManager* ResolveUIManagerFromActor(AActor* Candidate)
{
    if (!Candidate) return nullptr;

    if (auto* UI = Candidate->FindComponentByClass<UBPC_UIManager>())
        return UI;

    if (APawn* Pawn = Cast<APawn>(Candidate))
    {
        if (auto* UIp = Pawn->FindComponentByClass<UBPC_UIManager>())
            return UIp;

        if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
        {
            if (auto* UIpc = PC->FindComponentByClass<UBPC_UIManager>())
                return UIpc;

            if (APawn* PCPawn = PC->GetPawn())
                if (auto* UIpcp = PCPawn->FindComponentByClass<UBPC_UIManager>())
                    return UIpcp;
        }
    }

    if (APlayerController* PC2 = Cast<APlayerController>(Candidate))
    {
        if (auto* UIc = PC2->FindComponentByClass<UBPC_UIManager>())
            return UIc;

        if (APawn* PCPawn2 = PC2->GetPawn())
            if (auto* UIcp = PCPawn2->FindComponentByClass<UBPC_UIManager>())
                return UIcp;
    }

    return nullptr;
}

static UEquipmentComponent* ResolveEquipmentComponent(AActor* InstigatorActor, UInventoryComponent* Inventory)
{
    // 1) 인벤토리 소유자에서 먼저
    if (Inventory)
    {
        if (AActor* Owner = Inventory->GetOwner())
        {
            if (auto* Eq = Owner->FindComponentByClass<UEquipmentComponent>()) return Eq;

            if (auto* Pawn = Cast<APawn>(Owner))
            {
                if (auto* Eq2 = Pawn->FindComponentByClass<UEquipmentComponent>()) return Eq2;
                if (auto* PC = Cast<APlayerController>(Pawn->GetController()))
                {
                    if (APawn* PCPawn = PC->GetPawn())
                        if (auto* Eq3 = PCPawn->FindComponentByClass<UEquipmentComponent>()) return Eq3;
                }
            }
        }
    }

    // 2) Instigator에서도 시도 (PC 케이스 포함)
    if (InstigatorActor)
    {
        if (auto* Eq = InstigatorActor->FindComponentByClass<UEquipmentComponent>()) return Eq;

        if (auto* PC = Cast<APlayerController>(InstigatorActor))
        {
            if (APawn* P = PC->GetPawn())
                if (auto* Eq2 = P->FindComponentByClass<UEquipmentComponent>()) return Eq2;
        }

        if (auto* Pawn = Cast<APawn>(InstigatorActor))
        {
            if (auto* Eq2 = Pawn->FindComponentByClass<UEquipmentComponent>()) return Eq2;
            if (auto* PC = Cast<APlayerController>(Pawn->GetController()))
            {
                if (APawn* PCPawn = PC->GetPawn())
                    if (auto* Eq3 = PCPawn->FindComponentByClass<UEquipmentComponent>()) return Eq3;
            }
        }
    }

    return nullptr;
}

bool UItemUseLibrary::UseOrEquip(AActor* InstigatorActor, UInventoryComponent* Inventory, int32 SlotIndex)
{
    UE_LOG(LogTemp, Warning, TEXT("[UseOrEquip] ENTER  Slot=%d  Inv=%s (%p)  Instigator=%s (%p)"),
        SlotIndex,
        *GetNameSafe(Inventory), (void*)Inventory,
        *GetNameSafe(InstigatorActor), (void*)InstigatorActor);

    if (!Inventory)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UseOrEquip] Inventory=null"));
        return false;
    }

    UInventoryItem* Item = Inventory->GetAt(SlotIndex);
    if (!Item)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UseOrEquip] GetAt(%d)=null"), SlotIndex);
        return false;
    }

    const FItemRow& Row = Item->CachedRow;
    UE_LOG(LogTemp, Warning, TEXT("[UseOrEquip] Item=%s (%p)  RowId=%s  Type=%d  EquipSlot=%d  Qty=%d"),
        *GetNameSafe(Item), (void*)Item,
        *Item->ItemId.ToString(),
        (int32)Row.ItemType,
        (int32)Row.EquipSlot,
        Item->Quantity);

    // 1) 소모품
    if (Row.ItemType == EItemType::Consumable)
    {
        AActor* User = InstigatorActor ? InstigatorActor : Inventory->GetOwner();
        UE_LOG(LogTemp, Warning, TEXT("[UseOrEquip] -> UseConsumable via %s (%p)"),
            *GetNameSafe(User), (void*)User);
        return UseConsumable(User, Inventory, SlotIndex);
    }

    // 2) 장비 체크
    if (Row.EquipSlot == EEquipmentSlot::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UseOrEquip] Not equippable (EquipSlot=None)"));
        return false;
    }

    // 3) 어떤 EquipmentComponent를 쓸지 해석
    UEquipmentComponent* Equip = ResolveEquipmentComponent(InstigatorActor, Inventory);
    UE_LOG(LogTemp, Warning,
        TEXT("[UseOrEquip] Resolved EquipComp=%s (%p)  InvOwner=%s (%p)  Instigator=%s (%p)"),
        *GetNameSafe(Equip), (void*)Equip,
        *GetNameSafe(Inventory->GetOwner()), (void*)Inventory->GetOwner(),
        *GetNameSafe(InstigatorActor), (void*)InstigatorActor);

    if (!Equip)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UseOrEquip] EquipmentComponent NOT found -> abort"));
        return false;
    }

    AActor* EqOwner = Equip->GetOwner();
    UE_LOG(LogTemp, Warning, TEXT("[UseOrEquip] EquipOwner=%s (%p)  -> EquipFromInventory(From=%d, TargetSlot=%d)"),
        *GetNameSafe(EqOwner), (void*)EqOwner, SlotIndex, (int32)Row.EquipSlot);

    // 4) 실제 장착
    const bool bOk = Equip->EquipFromInventory(Inventory, SlotIndex, Row.EquipSlot);
    UE_LOG(LogTemp, Warning, TEXT("[UseOrEquip] EquipFromInventory => %s  (Equip=%p)"),
        bOk ? TEXT("OK") : TEXT("FAIL"), (void*)Equip);

    // 5) 장착 성공 시, 캐릭터창이 떠 있다면 강제 새로고침(더블클릭 안전망)
    if (bOk)
    {
        UBPC_UIManager* UI = nullptr;

        // 순서대로 첫 번째 발견된 UIManager 사용
        if (!UI) UI = ResolveUIManagerFromActor(InstigatorActor);
        if (!UI) UI = ResolveUIManagerFromActor(Inventory->GetOwner());
        if (!UI) UI = ResolveUIManagerFromActor(Equip->GetOwner());

        UE_LOG(LogTemp, Warning, TEXT("[UseOrEquip] UIManager resolved = %s (%p)"),
            *GetNameSafe(UI), (void*)UI);

        if (UI)
        {
            UI->RefreshCharacterEquipmentUI();
            UE_LOG(LogTemp, Warning, TEXT("[UseOrEquip] -> RefreshCharacterEquipmentUI() called"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[UseOrEquip] UIManager not found; UI refresh skipped"));
        }
    }

    return bOk;
}

bool UItemUseLibrary::UseConsumable(AActor* InstigatorActor, UInventoryComponent* Inventory, int32 SlotIndex)
{
    if (!Inventory) return false;
    UInventoryItem* Item = Inventory->GetAt(SlotIndex);
    if (!Item) return false;

    const FItemRow& Row = Item->CachedRow;
    if (Row.ItemType != EItemType::Consumable) return false;

    // 쿨그룹 확인
    const FName GroupId = Row.Consumable.CooldownGroupId;
    if (!GroupId.IsNone() && Inventory->IsCooldownActive(GroupId))
    {
        // 이미 쿨다운 중
        return false;
    }

    // 실제 효과 적용(GAS 예시 스텁)
    // - InstigatorActor에서 ASC를 찾아 GE 적용
    if (UAbilitySystemComponent* ASC = InstigatorActor ? InstigatorActor->FindComponentByClass<UAbilitySystemComponent>() : nullptr)
    {
        // 즉시 회복/지속 버프는 프로젝트의 GE 에셋/SetByCaller 정책에 맞춰 구현
        // 여기서는 간단히 성공했다고 가정
    }

    // 사용 성공 → 수량 차감
    Inventory->RemoveAt(SlotIndex, 1);

    // 쿨다운 시작
    if (!GroupId.IsNone() && Row.Consumable.CooldownTime > 0.f)
    {
        Inventory->StartCooldown(GroupId, Row.Consumable.CooldownTime);
    }
    return true;
}
