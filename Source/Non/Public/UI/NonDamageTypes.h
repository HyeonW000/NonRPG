#pragma once

#include "CoreMinimal.h"
#include "NonDamageTypes.generated.h"

/**
 * 데미지 숫자의 시각적 분류 (일반, 크리티컬, 회피 등)
 */
UENUM(BlueprintType)
enum class ENonDamageNumberCategory : uint8
{
    Normal,
    Critical,
    Dodge,
    Heal,
    PlayerDamage,
    Special
};
