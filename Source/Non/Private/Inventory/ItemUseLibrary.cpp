#include "Inventory/ItemUseLibrary.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryItem.h"
#include "Equipment/EquipmentComponent.h"
#include "AbilitySystemComponent.h"
#include "Core/NonUIManagerComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

static UNonUIManagerComponent* ResolveUIManagerFromActor(AActor* Candidate)
{
    if (!Candidate) return nullptr;

    if (auto* UI = Candidate->FindComponentByClass<UNonUIManagerComponent>())
        return UI;

    if (APawn* Pawn = Cast<APawn>(Candidate))
    {
        if (auto* UIp = Pawn->FindComponentByClass<UNonUIManagerComponent>())
            return UIp;

        if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
        {
            if (auto* UIpc = PC->FindComponentByClass<UNonUIManagerComponent>())
                return UIpc;

            if (APawn* PCPawn = PC->GetPawn())
                if (auto* UIpcp = PCPawn->FindComponentByClass<UNonUIManagerComponent>())
                    return UIpcp;
        }
    }

    if (APlayerController* PC2 = Cast<APlayerController>(Candidate))
    {
        if (auto* UIc = PC2->FindComponentByClass<UNonUIManagerComponent>())
            return UIc;

        if (APawn* PCPawn2 = PC2->GetPawn())
            if (auto* UIcp = PCPawn2->FindComponentByClass<UNonUIManagerComponent>())
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
    if (!Inventory)
    {
        return false;
    }

    UInventoryItem* Item = Inventory->GetAt(SlotIndex);
    if (!Item)
    {
        return false;
    }

    const FItemRow& Row = Item->CachedRow;

    // 1) 소모품
    if (Row.ItemType == EItemType::Consumable)
    {
        AActor* User = InstigatorActor ? InstigatorActor : Inventory->GetOwner();
        return UseConsumable(User, Inventory, SlotIndex);
    }

    // 2) 장비 체크
    if (Row.EquipSlot == EEquipmentSlot::None)
    {
        return false;
    }

    // 3) 어떤 EquipmentComponent를 쓸지 해석
    UEquipmentComponent* Equip = ResolveEquipmentComponent(InstigatorActor, Inventory);

    if (!Equip)
    {
        return false;
    }

    AActor* EqOwner = Equip->GetOwner();

    // 4) 실제 장착 (Server RPC 호출)
    // [Multiplayer Fixed] 로컬 함수(EquipFromInventory) 대신 서버 RPC 호출
    // ServerEquipFromInventory는 InventoryComponent를 '내 것'으로 가정하므로, 
    // Inventory == Equip->GetOwnerInventory()여야 안전함. (일반적인 경우 OK)
    Equip->ServerEquipFromInventory(SlotIndex, Row.EquipSlot);
    const bool bOk = true; // RPC는 즉시 결과를 알 수 없으므로 성공했다고 가정

    // 5) 장착 성공 시, 캐릭터창이 떠 있다면 강제 새로고침(더블클릭 안전망)
    if (bOk)
    {
        UNonUIManagerComponent* UI = nullptr;

        // 순서대로 첫 번째 발견된 UIManager 사용
        if (!UI) UI = ResolveUIManagerFromActor(InstigatorActor);
        if (!UI) UI = ResolveUIManagerFromActor(Inventory->GetOwner());
        if (!UI) UI = ResolveUIManagerFromActor(Equip->GetOwner());
        if (UI)
        {
            UI->RefreshCharacterEquipmentUI();
        }
        else
        {
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
