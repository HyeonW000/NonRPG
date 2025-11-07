#include "Animation/AnimSet_Weapon.h"

const FWeaponAnimSet& UAnimSet_Weapon::GetSetByStance(EWeaponStance Stance) const
{
    switch (Stance)
    {
    case EWeaponStance::OneHanded: return OneHanded;
    case EWeaponStance::TwoHanded: return TwoHanded;
    case EWeaponStance::Staff:     return Staff;
    default:                       return Unarmed;
    }
}
