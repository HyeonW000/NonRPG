#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Inventory/ItemEnums.h"
#include "ItemTypes.generated.h"

USTRUCT(BlueprintType)
struct FItemData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ItemId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Name;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;

    // 이미 ItemEnums.h에 정의된 타입을 사용
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EItemType ItemType = EItemType::Etc;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EEquipmentSlot EquipSlot = EEquipmentSlot::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EWeaponType WeaponType = EWeaponType::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EItemRarity Rarity = EItemRarity::Common;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EBindType BindType = EBindType::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MaxStack = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<USkeletalMesh> SkeletalMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UStaticMesh> StaticMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName AttachSocket = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly) float AttackPower = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float Defense = 0.f;
};
