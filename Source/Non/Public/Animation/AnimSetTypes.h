#pragma once

#include "UObject/ObjectMacros.h"
#include "AnimSetTypes.generated.h"

UENUM(BlueprintType)
enum class EWeaponStance : uint8
{
    Unarmed     UMETA(DisplayName = "Unarmed"),
    OneHanded   UMETA(DisplayName = "OneHanded"),
    TwoHanded   UMETA(DisplayName = "TwoHanded"),
    Staff       UMETA(DisplayName = "Staff")
};

USTRUCT(BlueprintType)
struct FHitReactSet
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<class UAnimMontage> Generic = nullptr;
};

USTRUCT(BlueprintType)
struct FDefenseSet
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<class UAnimMontage> Dodge = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<class UAnimMontage> Guard = nullptr;
};

USTRUCT(BlueprintType)
struct FAttackComboSet
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<class UAnimMontage> Combo1 = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<class UAnimMontage> Combo2 = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<class UAnimMontage> Combo3 = nullptr;
};

USTRUCT(BlueprintType)
struct FWeaponAnimSet
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equip")
    TObjectPtr<class UAnimMontage> Equip = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equip")
    TObjectPtr<class UAnimMontage> Unequip = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
    FAttackComboSet Attacks;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense")
    FDefenseSet Defense;
};

USTRUCT(BlueprintType)
struct FClassCommonSet
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion")
    TObjectPtr<class UBlendSpace> BS_IdleRun = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death")
    TObjectPtr<class UAnimMontage> DeathMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact")
    FHitReactSet HitReact;
};
