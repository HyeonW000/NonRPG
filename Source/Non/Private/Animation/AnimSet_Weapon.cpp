#include "Animation/AnimSet_Weapon.h"

const FWeaponAnimSet& UAnimSet_Weapon::GetSetByStance(EWeaponStance Stance) const
{
    switch (Stance)
    {
    case EWeaponStance::SwordShield: return SwordShield;
    case EWeaponStance::Greatsword:  return Greatsword;
    case EWeaponStance::Staff:     return Staff;
    default:                       return Unarmed;
    }
}
