#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Skill/SkillTypes.h"
#include "NonSaveGame.generated.h"

/**
 * 인벤토리 아이템 저장용 구조체
 */
USTRUCT(BlueprintType)
struct FInventorySaveData
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadWrite)
    FName ItemId;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 Quantity = 0;
};

/**
 * 게임 저장 데이터 클래스
 */
UCLASS()
class NON_API UNonSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Player")
    FString PlayerName = TEXT("Player");

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Player")
    EJobClass JobClass = EJobClass::Defender;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Player")
    int32 Level = 1;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Player")
    int32 EXP = 0;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Player")
    FTransform PlayerTransform;

    /* 스킬 데이터 */
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Skill")
    int32 SkillPoints = 0;

    // 스킬 ID -> 레벨 매핑
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Skill")
    TMap<FName, int32> SkillLevels;

    /* 인벤토리 데이터 */
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Inventory")
    TArray<FInventorySaveData> InventoryItems;
};
