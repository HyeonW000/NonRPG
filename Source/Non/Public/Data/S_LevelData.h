#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "S_LevelData.generated.h"

USTRUCT(BlueprintType)
struct FLevelData : public FTableRowBase
{
    GENERATED_BODY()


    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxHP;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxMP;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExpToNextLevel;

};
