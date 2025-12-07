#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AI/EnemyCharacter.h" 
#include "EnemyDataAsset.generated.h"

class AActor;

// 개별 드롭 아이템 정보
USTRUCT(BlueprintType)
struct FEnemyDropItem
{
    GENERATED_BODY()

    // DT_ItemData 안의 RowName (아이템 ID)
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ItemId;

    // 드롭 확률 (0.0 ~ 1.0)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DropChance = 0.5f;

    // 드롭 개수 범위
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MinCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaxCount = 1;
};

UCLASS(BlueprintType)
class NON_API UEnemyDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    // 에디터에서 식별용 이름(옵션)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ID")
    FName EnemyId;

    // 어떤 Enemy BP를 스폰할지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class")
    TSubclassOf<AEnemyCharacter> EnemyClass;

    //  이 적이 참조할 아이템 데이터 테이블 (DT_ItemData)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drops")
    UDataTable* ItemDataTable;

    //  어떤 아이템을 드롭할지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drops")
    TArray<FEnemyDropItem> DropList;

    // 기본 스탯
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    float MaxHP = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    float Attack = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    float Defense = 5.f;

    // 처치 시 경험치
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
    float ExpReward = 10.f;
};