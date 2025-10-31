#pragma once

#include "CombatTypes.generated.h"

// 방향 4분면 (공용)
UENUM(BlueprintType)
enum class EHitQuadrant : uint8
{
    Front,
    Back,
    Left,
    Right
};

// 넉백 방식 (공용) — 이미 NonCharacterBase에 있다면 이걸로 통일
UENUM(BlueprintType)
enum class EKnockbackMode : uint8
{
    None,
    Launch,     // 코드 LaunchCharacter
    RootMotion  // 애니 루트모션
};
