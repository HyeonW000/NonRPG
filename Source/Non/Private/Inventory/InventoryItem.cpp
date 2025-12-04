#include "Inventory/InventoryItem.h"
#include "Inventory/ItemEnums.h"
#include "Data/ItemStructs.h"
#include "Engine/DataTable.h"

void UInventoryItem::Init(FName InItemId, int32 InQty, UDataTable* DT)
{
    ItemId = InItemId;
    Quantity = FMath::Max(1, InQty);
    InstanceId = FGuid::NewGuid();
    ItemDataTable = DT;

    if (ItemDataTable && !ItemId.IsNone())
    {
        if (const FItemRow* Row = ItemDataTable->FindRow<FItemRow>(ItemId, TEXT("UInventoryItem::Init")))
        {
            CachedRow = *Row;
            Quantity = FMath::Clamp(Quantity, 1, FMath::Max(1, CachedRow.MaxStack));
        }
    }
}

bool UInventoryItem::IsEquipment() const
{
     return CachedRow.ItemType == EItemType::Equipment;

}
EEquipmentSlot UInventoryItem::GetEquipSlot() const
{
    // 장비가 아니면 슬롯 없음
    if (CachedRow.ItemType != EItemType::Equipment)
    {
        return EEquipmentSlot::None;
    }

    // 데이터 행에 명시된 슬롯이 최우선
    if (CachedRow.EquipSlot != EEquipmentSlot::None)
    {
        return CachedRow.EquipSlot;
    }

    // (폴백) 무기 타입이 지정되어 있으면 메인 무기로 취급
    if (CachedRow.WeaponType != EWeaponType::None)
    {
        return EEquipmentSlot::WeaponMain;
    }

    // 필요하다면 여기서 ArmorType/AccessoryType 등으로 더 매핑해도 됨

    return EEquipmentSlot::None;
}

bool UInventoryItem::IsTwoHandedWeapon() const
{
    // 장비 + 무기타입=TwoHanded 일 때만 true
    return (CachedRow.ItemType == EItemType::Equipment) &&
        (CachedRow.WeaponType == EWeaponType::TwoHanded);
}