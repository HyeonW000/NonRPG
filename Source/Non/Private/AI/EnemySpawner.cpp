#include "AI/EnemySpawner.h"
#include "AI/EnemyCharacter.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Data/EnemyDataAsset.h"

namespace
{
    // 지면 스냅: 위/아래로 라인트레이스해서 바닥 위치를 구함
    static bool FindGround(UWorld* World, const FVector& Probe, float Up, float Down, FHitResult& OutHit)
    {
        if (!World) return false;
        const FVector Start = Probe + FVector(0, 0, Up);
        const FVector End = Probe - FVector(0, 0, Down);

        FCollisionQueryParams P(SCENE_QUERY_STAT(EnemySpawnerFindGround), false);
        P.bReturnPhysicalMaterial = false;

        return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, P);
    }
}

AEnemySpawner::AEnemySpawner()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AEnemySpawner::BeginPlay()
{
    Super::BeginPlay();
    TryInitialSpawn();

    // 상시 리필 시작
    if (bAutoRefill && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            RefillTimer, this, &AEnemySpawner::RefillTick, RefillInterval, true, RefillInterval);
    }
}

void AEnemySpawner::TryInitialSpawn()
{
    for (int32 i = 0; i < InitialCount; ++i)
    {
        TrySpawnOne();
    }
}

// 실제 스폰: 캡슐 반높이만큼 올린 뒤 "충돌나면 스폰하지 않음" 정책
void AEnemySpawner::TrySpawnOne()
{
    //  DataAsset/EnemyClass 체크
    if (!GetWorld() || !EnemyDataAsset || !EnemyDataAsset->EnemyClass) return;

    // 살아있는 목록 정리 + MaxAlive 체크
    Alive.RemoveAll([](const TWeakObjectPtr<AEnemyCharacter>& P) { return !P.IsValid(); });
    if (Alive.Num() >= MaxAlive) return;

    FVector SpawnLoc = PickSpawnPoint();
    FRotator SpawnRot(0.f, FMath::FRandRange(-180.f, 180.f), 0.f);

    // 캡슐 반높이/반지름 읽어서 지면 위로 올림
    float HalfHeight = 88.f, Radius = 34.f;
    if (const AEnemyCharacter* Def = EnemyDataAsset->EnemyClass->GetDefaultObject<AEnemyCharacter>())
    {
        if (const UCapsuleComponent* Cap = Def->GetCapsuleComponent())
        {
            HalfHeight = Cap->GetUnscaledCapsuleHalfHeight();
            Radius = Cap->GetUnscaledCapsuleRadius();
        }
    }
    SpawnLoc.Z += HalfHeight + 2.f; // 살짝 여유

    // 충돌나면 스폰하지 않음(끼임 방지)
    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

    AEnemyCharacter* E = GetWorld()->SpawnActor<AEnemyCharacter>(EnemyDataAsset->EnemyClass, SpawnLoc, SpawnRot, SP);
    if (!E) return; // 충돌이라 포기 → 다음 리필틱/데스 리스폰 때 재시도됨

    //  DataAsset 값 적용
    E->InitFromDataAsset(EnemyDataAsset);

    Alive.Add(E);

    if (!E->GetController())
    {
        E->SpawnDefaultController();
    }

    // 죽음 이벤트 구독(이미 있으면 유지)
    E->OnEnemyDied.AddDynamic(this, &AEnemySpawner::OnEnemyDied);
}

// 지점을 고르는 로직: NavMesh 투영 → 지면 스냅
FVector AEnemySpawner::PickSpawnPoint() const
{
    FVector Center = GetActorLocation();
    FVector Candidate = Center;

    // 원형 랜덤 오프셋
    if (SpawnRadius > 10.f)
    {
        const FVector Rand2D = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.f, SpawnRadius);
        Candidate += FVector(Rand2D.X, Rand2D.Y, 0.f);
    }

    // 네브메시에 투영(있으면)
    if (bProjectToNavMesh)
    {
        if (UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld()))
        {
            FNavLocation Out;
            if (Nav->ProjectPointToNavigation(Candidate, Out, FVector(SpawnRadius, SpawnRadius, 500.f)))
            {
                Candidate = Out.Location;
            }
        }
    }

    // 지면 스냅(위로 1000, 아래로 2000)
    FHitResult Hit;
    if (FindGround(GetWorld(), Candidate, /*Up*/1000.f, /*Down*/2000.f, Hit))
    {
        Candidate = Hit.ImpactPoint;
    }

    return Candidate;
}

void AEnemySpawner::OnEnemyDied(AEnemyCharacter* Dead)
{
    if (!GetWorld()) return;

    Alive.RemoveAll([Dead](const TWeakObjectPtr<AEnemyCharacter>& P) {
        return !P.IsValid() || P.Get() == Dead;
        });

    // 죽은 뒤에도 한 마리 리스폰 예약(개별 보충)
    GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AEnemySpawner::TrySpawnOne, RespawnDelay, false);
}

// 주기적으로 한 마리씩 보충 → 시간이 지나면 자연스럽게 MaxAlive에 도달
void AEnemySpawner::RefillTick()
{
    TrySpawnOne();
}
