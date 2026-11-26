#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.generated.h"

/** AOE 형태 (DamageAOE, 스킬 AOE 설정, 애님 노티파이 공용. 이전 EAOEShape 통합) */
UENUM(BlueprintType)
enum class EAOEShape : uint8
{
    Box      UMETA(DisplayName = "Box (상자형)"),
    Sphere   UMETA(DisplayName = "Sphere (구형)"),
    Capsule  UMETA(DisplayName = "Capsule (캡슐형)")
};

/** 팀 구분 (이전 ETeamSide, ETeamSide 통합) */
UENUM(BlueprintType)
enum class ETeamSide : uint8
{
    Neutral,
    Player,
    Enemy
};

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
