#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class AEnemyCharacter;

USTRUCT(BlueprintType)
struct FEnemySpawnEntry
{
    GENERATED_BODY()

    // 몬스터 행 이름 (DT_EnemyData의 RowName - 드롭다운으로 선택 가능)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (GetOptions = "GetEnemyRowNames"))
    FName RowName = NAME_None;

    // 고정 스폰 마릿수 (0이면 비율(Weight) 기반으로 스폰, 1 이상이면 필드에 해당 마릿수를 항상 유지)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
    int32 FixedCount = 0;

    // 스폰 가중치/비율 (FixedCount가 0인 몬스터끼리 남은 자리를 채울 때 사용되는 비율, 예: 검사 3, 궁수 1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1"))
    int32 Weight = 1;
};

UCLASS()
class NON_API AEnemySpawner : public AActor
{
    GENERATED_BODY()
public:
    AEnemySpawner();

    // 몬스터 데이터 테이블 (DT_EnemyData)
    UPROPERTY(EditAnywhere, Category = "Spawn")
    UDataTable* EnemyDataTable;

    // [스폰 목록] 스폰할 몬스터와 수량/비율을 등록하는 통합 목록 (드롭다운으로 간편 선택)
    UPROPERTY(EditAnywhere, Category = "Spawn", meta = (EditCondition = "EnemyDataTable != nullptr"))
    TArray<FEnemySpawnEntry> SpawnEntries;

    UFUNCTION()
    TArray<FString> GetEnemyRowNames() const;

    // 게임 시작 시 즉시 스폰할 마릿수 (FixedCount를 사용 중이라면 그 총합만큼 자동 보장되므로 비워두거나 신경 쓰지 않으셔도 됩니다)
    UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ToolTip = "게임 시작 시 즉시 스폰할 마릿수 (FixedCount 사용 시 총합만큼 자동 스폰)"))
    int32 InitialCount = 3;

    // 스포너 구역에 유지될 최대 몬스터 정원 (비율 스폰 시 전체 정원 기준. FixedCount 총합이 더 크면 자동으로 확장되어 보장됩니다)
    UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ToolTip = "스포너 구역에 유지될 최대 몬스터 수 (FixedCount 총합보다 작으면 자동 확장)"))
    int32 MaxAlive = 5;

    // 죽은 뒤 해당 딜레이 후 1마리 리스폰
    UPROPERTY(EditAnywhere, Category = "Spawn")
    float RespawnDelay = 5.f;

    // 스포너 중심 원 반경
    UPROPERTY(EditAnywhere, Category = "Spawn")
    float SpawnRadius = 800.f;

    // 네브메시에 투영할지
    UPROPERTY(EditAnywhere, Category = "Spawn")
    bool bProjectToNavMesh = true;

    // --- 상시 리필 옵션 ---
    UPROPERTY(EditAnywhere, Category = "Spawn|Refill")
    bool bAutoRefill = true;

    // 리필 주기(초). 주기마다 TrySpawnOne()을 호출해서 MaxAlive까지 보충
    UPROPERTY(EditAnywhere, Category = "Spawn|Refill", meta = (EditCondition = "bAutoRefill", ClampMin = "0.1"))
    float RefillInterval = 3.f;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY()
    TArray<TWeakObjectPtr<AEnemyCharacter>> Alive;

    // 리스폰 딜레이(RespawnDelay) 대기 중인 몬스터 행 이름 목록 (조기 리필 방지용)
    UPROPERTY()
    TArray<FName> PendingRespawnRows;

    FTimerHandle RefillTimer;

    void TryInitialSpawn();
    void TrySpawnOne();
    FVector PickSpawnPoint() const;

    UFUNCTION()
    void OnEnemyDied(AEnemyCharacter* Dead);

    // RespawnDelay 만료 시 호출될 콜백
    void OnRespawnTimerExpired(FName TargetRowName);

    // 주기적으로 MaxAlive까지 보충
    void RefillTick();
};
