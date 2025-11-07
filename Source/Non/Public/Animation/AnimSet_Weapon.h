#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimSetTypes.h"
#include "AnimSet_Weapon.generated.h"

/** 무기(스탠스)별 애니메이션 세트 DataAsset */
UCLASS(BlueprintType)
class UAnimSet_Weapon : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OneHanded")
    FWeaponAnimSet OneHanded;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TwoHanded")
    FWeaponAnimSet TwoHanded;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Staff")
    FWeaponAnimSet Staff;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unarmed")
    FWeaponAnimSet Unarmed;

    UFUNCTION(BlueprintPure)
    const FWeaponAnimSet& GetSetByStance(EWeaponStance Stance) const;
};
