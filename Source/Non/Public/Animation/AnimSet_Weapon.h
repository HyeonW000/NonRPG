#pragma once

#include "Engine/DataAsset.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSetTypes.h"
#include "AnimSet_Weapon.generated.h"

UCLASS(BlueprintType)
class NON_API UAnimSet_Weapon : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    FWeaponAnimSet Unarmed;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    FWeaponAnimSet OneHanded;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    FWeaponAnimSet TwoHanded;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    FWeaponAnimSet Staff;

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    const FWeaponAnimSet& GetSetByStance(EWeaponStance Stance) const
    {
        switch (Stance)
        {
        case EWeaponStance::OneHanded: return OneHanded;
        case EWeaponStance::TwoHanded: return TwoHanded;
        case EWeaponStance::Staff:     return Staff;
        default:                     return Unarmed;
        }
    }
};
