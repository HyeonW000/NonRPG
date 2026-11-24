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

// 무기 종류. 이름은 State.Armed.* 태그, 어빌리티 폴더(GA/Player/Sword 등)와 같은 단어를 쓴다.
// 예전 이름(OneHanded/TwoHanded/WeaponSub)은 DefaultEngine.ini [CoreRedirects]로 자동 변환됨
UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    None,
    Sword       UMETA(DisplayName = "한손검"),
    GreatSword  UMETA(DisplayName = "대검"),
    Shield      UMETA(DisplayName = "방패"),
    Staff       UMETA(DisplayName = "스태프")
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
