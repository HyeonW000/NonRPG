#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "LevelData.generated.h"

USTRUCT(BlueprintType)
struct FLevelData : public FTableRowBase
{
    GENERATED_BODY()

    // 생성자를 추가하여 모든 변수를 초기화합니다.
    FLevelData()
        : MaxHP(0.0f)
        , MaxMP(0.0f)
        , ExpToNextLevel(0.0f)
    {
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxHP;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxMP;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExpToNextLevel;
};