#pragma once
#include "ItemEnums.generated.h"

UENUM(BlueprintType)
enum class EItemType : uint8
{
    Equipment,
    Consumable,
    Material,
    Quest,
    Cosmetic,
    Etc
};

UENUM(BlueprintType)
enum class EEquipmentSlot : uint8
{
    None,
    Head,
    Chest,
    Legs,
    Hands,
    Feet,
    WeaponMain,
    WeaponSub,
    Accessory1,
    Accessory2
};

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    None,
    OneHanded,
    TwoHanded,
    WeaponSub,
    Staff
    // 필요 시 Spear, Bow 등 추가
};

UENUM(BlueprintType)
enum class EItemRarity : uint8
{
    Common,
    Uncommon,
    Rare,
    Epic,
    Legendary,
    Mythic
};

UENUM(BlueprintType)
enum class EBindType : uint8
{
    None,
    BindOnPickup,
    BindOnEquip,
    Tradable
};
