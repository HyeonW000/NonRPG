#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "AnimSetTypes.generated.h"

/** 무기 스탠스 */
UENUM(BlueprintType)
enum class EWeaponStance : uint8
{
    Unarmed,
    OneHanded,
    TwoHanded,
    Staff
};

/** 콤보 묶음 */
USTRUCT(BlueprintType)
struct FComboAnimSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) UAnimMontage* Combo1 = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) UAnimMontage* Combo2 = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) UAnimMontage* Combo3 = nullptr;
};

/** 8방향(0:F,1:FR,2:R,3:BR,4:B,5:BL,6:L,7:FL) */
USTRUCT(BlueprintType)
struct FDirectional8Montages
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) UAnimMontage* F = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) UAnimMontage* FR = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) UAnimMontage* R = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) UAnimMontage* BR = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) UAnimMontage* B = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) UAnimMontage* BL = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) UAnimMontage* L = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) UAnimMontage* FL = nullptr;

    FORCEINLINE UAnimMontage* GetByIndex(int32 Idx) const
    {
        switch (Idx & 7)
        {
        case 0: return F;   case 1: return FR;  case 2: return R;   case 3: return BR;
        case 4: return B;   case 5: return BL;  case 6: return L;   default: return FL;
        }
    }
};

/** 무기 세트(스탠스별) */
USTRUCT(BlueprintType)
struct FWeaponAnimSet
{
    GENERATED_BODY()

    // Draw / Sheathe
    UPROPERTY(EditAnywhere, BlueprintReadOnly) UAnimMontage* Equip = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) UAnimMontage* Unequip = nullptr;

    // 공격(콤보)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
    FComboAnimSet Attacks;

    // 8방향 회피(무기별)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge")
    FDirectional8Montages Dodge;

    // 무기별 히트리액트(경/중)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
    UAnimMontage* HitReact_Light = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
    UAnimMontage* HitReact_Heavy = nullptr;
};

USTRUCT(BlueprintType)
struct FCommonAnimSet
{
    GENERATED_BODY()

    // 공통 히트리액트(무기별 히트리액트가 없을 때 폴백)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
    UAnimMontage* HitReact_Generic = nullptr;

    // 공통 데스 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
    UAnimMontage* DeathMontage = nullptr;

    // (필요하면 Idle/Run 등 나중에 확장 가능)
};