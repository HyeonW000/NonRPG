#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class AEnemyCharacter;

UCLASS()
class NON_API AEnemySpawner : public AActor
{
    GENERATED_BODY()
public:
    AEnemySpawner();

    UPROPERTY(EditAnywhere, Category = "Spawn")
    TSubclassOf<AEnemyCharacter> EnemyClass;

    UPROPERTY(EditAnywhere, Category = "Spawn")
    int32 InitialCount = 3;

    UPROPERTY(EditAnywhere, Category = "Spawn")
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

    // --- 추가: 상시 리필 옵션 ---
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

    FTimerHandle RespawnTimer;
    FTimerHandle RefillTimer;   // 추가

    void TryInitialSpawn();
    void TrySpawnOne();
    FVector PickSpawnPoint() const;

    UFUNCTION()
    void OnEnemyDied(AEnemyCharacter* Dead);

    // 추가: 주기적으로 MaxAlive까지 보충
    void RefillTick();
};
