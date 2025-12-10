#pragma once

#include "CoreMinimal.h"
#include "UITypes.generated.h"

UENUM(BlueprintType)
enum class EGameWindowType : uint8
{
    None        UMETA(DisplayName = "None"),
    Inventory   UMETA(DisplayName = "Inventory"),
    Character   UMETA(DisplayName = "Character"),
    Skill       UMETA(DisplayName = "Skill"),
    SystemMenu  UMETA(DisplayName = "SystemMenu"),
    
    // Future windows...
    Quest       UMETA(DisplayName = "Quest"),
    Party       UMETA(DisplayName = "Party"),
    Guild       UMETA(DisplayName = "Guild"),
    Setting     UMETA(DisplayName = "Setting")
};
