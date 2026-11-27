#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Skill/SkillTypes.h"
#include "Inventory/ItemEnums.h"
#include "NonSaveGame.generated.h"

/**
 * 세이브 형식 버전. 칸을 추가·변경할 때 올리고 UNonSaveGame::MigrateFromOldVersion에 변환을 추가한다.
 *  0: SaveVersion 칸이 없던 옛 세이브
 *  1: 2026-11-28 — 골드, 저장한 맵, 아이템 고유 ID·강화·칸 번호, 개인 월드 기록, 덧입기 (NON-DIS-2)
 */
namespace NonSaveVersion
{
    constexpr int32 Current = 1;
}

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

    /** 인벤토리 칸 번호 (-1이면 빈 칸에 차례로 넣음) */
    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 SlotIndex = INDEX_NONE;

    /** 아이템 하나하나의 고유 ID (중복 아이템 방지, 파티 내 양도 검증) */
    UPROPERTY(SaveGame, BlueprintReadWrite)
    FGuid InstanceId;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 EnhancementLevel = 0;

    /** 강화 천장 게이지 (0~1, NON-DIS-1) */
    UPROPERTY(SaveGame, BlueprintReadWrite)
    float EnhanceGauge = 0.f;
};

/**
 * 퀵슬롯 저장용 구조체
 */
USTRUCT(BlueprintType)
struct FQuickSlotSaveData
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 SlotIndex = 0;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    FName ItemId = NAME_None;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    FName SkillId = NAME_None;
};

/**
 * 장비 저장용 구조체
 */
USTRUCT(BlueprintType)
struct FEquipmentSaveData
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadWrite)
    EEquipmentSlot Slot = EEquipmentSlot::None;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    FName ItemId = NAME_None;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 Quantity = 1;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    FGuid InstanceId;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 EnhancementLevel = 0;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    float EnhanceGauge = 0.f;
};

/** 스킬 레벨 한 줄 (RPC로 보낼 때 TMap 대신 사용) */
USTRUCT(BlueprintType)
struct FSkillLevelSaveEntry
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadWrite)
    FName SkillId = NAME_None;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    int32 Level = 0;
};

/**
 * 개인 월드 기록 (NON-DIS-12 "세션은 방장, 기록은 각자").
 * 친구 방에서 한 것도 내 기록에 남는다. 칸만 먼저 두고 내용은 각 기능을 만들 때 채운다.
 */
USTRUCT(BlueprintType)
struct FPersonalRecordSaveData
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadWrite)
    TArray<FName> UnlockedWaypoints;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    TArray<FName> DefeatedBosses;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    TArray<FName> OpenedChests;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    TArray<FName> CompletedQuests;

    /** 탐험 도감: 발견한 장소 */
    UPROPERTY(SaveGame, BlueprintReadWrite)
    TArray<FName> DiscoveredPlaces;

    /** 탐험 도감: 몬스터 종류별 처치 수 */
    UPROPERTY(SaveGame, BlueprintReadWrite)
    TMap<FName, int32> MonsterKillCounts;

    UPROPERTY(SaveGame, BlueprintReadWrite)
    bool bRedMoonUnlocked = false;
};

/**
 * 게임 저장 데이터 클래스
 */
UCLASS()
class NON_API UNonSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    /** 불러올 때 옛 형식을 현재 형식으로 바꾼다 (버전 변환 + 옛 아이템 ID). 모든 LoadGameFromSlot 경로에 적용됨 */
    virtual void Serialize(FArchive& Ar) override;

    /** 인벤토리·장비·퀵슬롯의 아이템 ID를 현재 규칙의 ID로 바꾼다. 바뀐 개수를 돌려줌 */
    int32 MigrateItemIds();

    /** SaveVersion이 현재보다 낮으면 차례로 변환한다. 변환했으면 true */
    bool MigrateFromOldVersion();

public:
    /** 세이브 형식 버전. 옛 파일에는 이 칸이 없어서 0으로 읽힘. 저장할 때 NonSaveVersion::Current로 맞춘다 */
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Meta")
    int32 SaveVersion = 0;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Player")
    FString PlayerName = TEXT("Player");

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Player")
    EJobClass JobClass = EJobClass::Defender;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Player")
    int32 Level = 1;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Player")
    int32 EXP = 0;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Player")
    float CurrentHP = 100.f;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Player")
    float CurrentMP = 100.f;

    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Player")
    int32 Gold = 0;

    /** 저장할 때 있던 맵. 같은 맵으로 들어올 때만 PlayerTransform을 쓴다 (다른 맵·파티 합류는 시작 지점) */
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Player")
    FName SavedMapName = NAME_None;

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

    /* 퀵슬롯 데이터 */
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "QuickSlot")
    TArray<FQuickSlotSaveData> QuickSlots;

    /* 장비 데이터 */
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Equipment")
    TArray<FEquipmentSaveData> EquippedItems;

    /** 덧입기(외형 덮어쓰기) — 장비 슬롯별 외형 ID. 스탯 없음 (NON-DIS-11) */
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Cosmetic")
    TMap<EEquipmentSlot, FName> CosmeticOverrides;

    /* 개인 월드 기록 */
    UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Record")
    FPersonalRecordSaveData PersonalRecords;
};
