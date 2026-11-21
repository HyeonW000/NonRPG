#include "AI/EnemySpawner.h"
#include "Character/EnemyCharacter.h"
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

TArray<FString> AEnemySpawner::GetEnemyRowNames() const
{
    TArray<FString> Options;
    if (EnemyDataTable)
    {
        for (const FName& RowName : EnemyDataTable->GetRowNames())
        {
            Options.Add(RowName.ToString());
        }
    }
    return Options;
}

void AEnemySpawner::BeginPlay()
{
    Super::BeginPlay();

    // 권한 확인: 스포너 로직(스폰, 리필)은 서버에서만 실행
    if (!HasAuthority()) return;

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
    // 고정 스폰 마릿수(FixedCount)의 총합을 계산하여 최소한 고정 마릿수는 모두 스폰되도록 보장
    int32 TotalFixed = 0;
    for (const FEnemySpawnEntry& Entry : SpawnEntries)
    {
        TotalFixed += FMath::Max(0, Entry.FixedCount);
    }
    const int32 TargetInitial = FMath::Max(InitialCount, TotalFixed);

    for (int32 i = 0; i < TargetInitial; ++i)
    {
        TrySpawnOne();
    }
}

// 실제 스폰: 캡슐 반높이만큼 올린 뒤 "충돌나면 스폰하지 않음" 정책
void AEnemySpawner::TrySpawnOne()
{
    if (!EnemyDataTable || SpawnEntries.Num() == 0) return;

    // 살아있는 목록 정리
    Alive.RemoveAll([](const TWeakObjectPtr<AEnemyCharacter>& P) { return !P.IsValid(); });

    // 고정 스폰 마릿수의 총합이 MaxAlive보다 크다면 MaxAlive를 자동으로 확장하여 의도한 고정 마릿수 보장
    int32 TotalFixed = 0;
    for (const FEnemySpawnEntry& Entry : SpawnEntries)
    {
        TotalFixed += FMath::Max(0, Entry.FixedCount);
    }
    const int32 EffectiveMaxAlive = FMath::Max(MaxAlive, TotalFixed);

    // 살아있는 몬스터 수 + 리스폰 대기 중(Pending)인 몬스터 수의 합이 정원 이상이면 스폰 차단
    const int32 TotalActiveCount = Alive.Num() + PendingRespawnRows.Num();
    if (TotalActiveCount >= EffectiveMaxAlive) return;

    FName TargetRowName = NAME_None;

    // 1단계 [고정 수(FixedCount) 우선]: FixedCount > 0으로 설정된 몬스터 중 필드에 부족한 종류를 최우선 선택
    FName DeficitRowName = NAME_None;
    int32 MaxDeficit = 0;

    for (const FEnemySpawnEntry& Entry : SpawnEntries)
    {
        if (Entry.RowName.IsNone() || Entry.FixedCount <= 0) continue;

        int32 CurrentCount = 0;
        // 1) 현재 필드에 살아있는 수
        for (const auto& P : Alive)
        {
            if (P.IsValid() && P->EnemyRowName == Entry.RowName)
            {
                CurrentCount++;
            }
        }
        // 2) 리스폰 딜레이를 기다리는 중인 수
        for (const FName& PendingRow : PendingRespawnRows)
        {
            if (PendingRow == Entry.RowName)
            {
                CurrentCount++;
            }
        }

        const int32 Deficit = Entry.FixedCount - CurrentCount;
        if (Deficit > MaxDeficit)
        {
            MaxDeficit = Deficit;
            DeficitRowName = Entry.RowName;
        }
    }

    if (!DeficitRowName.IsNone())
    {
        TargetRowName = DeficitRowName;
    }
    else
    {
        // 2단계 [비율(Weight) 추첨]: 고정 수가 모두 충족되었거나 설정되지 않은 슬롯에 대해 가중치 기반 랜덤 선택
        bool bHasZeroFixedEntries = false;
        for (const FEnemySpawnEntry& Entry : SpawnEntries)
        {
            if (!Entry.RowName.IsNone() && Entry.FixedCount == 0)
            {
                bHasZeroFixedEntries = true;
                break;
            }
        }

        int32 TotalWeight = 0;
        for (const FEnemySpawnEntry& Entry : SpawnEntries)
        {
            if (Entry.RowName.IsNone()) continue;
            // FixedCount == 0인 일반 슬롯 항목이 따로 있다면, FixedCount > 0인 고정 항목은 추가 스폰되지 않도록 제외
            if (bHasZeroFixedEntries && Entry.FixedCount > 0) continue;

            TotalWeight += FMath::Max(1, Entry.Weight);
        }

        if (TotalWeight > 0)
        {
            int32 RandomWeight = FMath::RandRange(1, TotalWeight);
            int32 AccumulatedWeight = 0;
            for (const FEnemySpawnEntry& Entry : SpawnEntries)
            {
                if (Entry.RowName.IsNone()) continue;
                if (bHasZeroFixedEntries && Entry.FixedCount > 0) continue;

                AccumulatedWeight += FMath::Max(1, Entry.Weight);
                if (RandomWeight <= AccumulatedWeight)
                {
                    TargetRowName = Entry.RowName;
                    break;
                }
            }
        }
    }

    if (TargetRowName.IsNone()) return;

    const FEnemyDataTableRow* FoundRow = EnemyDataTable->FindRow<FEnemyDataTableRow>(TargetRowName, TEXT("EnemySpawner::TrySpawnOne"));
    if (!FoundRow || !FoundRow->EnemyClass) return;

    TSubclassOf<AEnemyCharacter> SpawnClass = FoundRow->EnemyClass;
    if (!GetWorld() || !SpawnClass) return;

    FVector SpawnLoc = PickSpawnPoint();
    FRotator SpawnRot(0.f, FMath::FRandRange(-180.f, 180.f), 0.f);

    // 캡슐 반높이/반지름 읽어서 지면 위로 올림
    float HalfHeight = 88.f, Radius = 34.f;
    if (const AEnemyCharacter* Def = SpawnClass->GetDefaultObject<AEnemyCharacter>())
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

    AEnemyCharacter* E = GetWorld()->SpawnActor<AEnemyCharacter>(SpawnClass, SpawnLoc, SpawnRot, SP);
    if (!E) return; // 충돌이라 포기 → 다음 리필틱/데스 리스폰 때 재시도됨

    // 스폰된 몬스터의 행 이름 명시적 기록 (하이브리드 카운팅용)
    E->EnemyRowName = TargetRowName;

    // 데이터 테이블 행으로부터 스탯 및 드랍템 자동 주입
    E->InitFromDataTableRow(*FoundRow);

    // EnemyData가 세팅된 이후에 Attribute 초기화
    E->InitializeAttributes();

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
    if (!GetWorld() || !Dead) return;

    const FName DeadRowName = Dead->EnemyRowName;

    Alive.RemoveAll([Dead](const TWeakObjectPtr<AEnemyCharacter>& P) {
        return !P.IsValid() || P.Get() == Dead;
    });

    if (RespawnDelay > 0.0f)
    {
        // ★ 리스폰 딜레이 동안 정원 카운트를 유지하도록 대기 목록에 등록 (조기 리필 방지!)
        PendingRespawnRows.Add(DeadRowName);

        FTimerHandle NewRespawnTimer;
        FTimerDelegate Delegate;
        Delegate.BindUObject(this, &AEnemySpawner::OnRespawnTimerExpired, DeadRowName);
        GetWorld()->GetTimerManager().SetTimer(NewRespawnTimer, Delegate, RespawnDelay, false);

        UE_LOG(LogTemp, Warning, TEXT("[스포너 디버그] %s 사망 -> 리스폰 딜레이 %.1f초 대기 시작 (대기 중: %d마리)"), 
            *DeadRowName.ToString(), RespawnDelay, PendingRespawnRows.Num());
    }
    else
    {
        // 딜레이가 0초면 즉시 스폰 시도
        TrySpawnOne();
    }
}

void AEnemySpawner::OnRespawnTimerExpired(FName TargetRowName)
{
    // 대기 목록에서 1개 제거하고 스폰 실행
    PendingRespawnRows.RemoveSingle(TargetRowName);

    UE_LOG(LogTemp, Warning, TEXT("[스포너 디버그] %s 리스폰 딜레이 완료 -> 리스폰 실행!"), *TargetRowName.ToString());
    TrySpawnOne();
}

// 주기적으로 한 마리씩 보충 → 시간이 지나면 자연스럽게 MaxAlive에 도달
void AEnemySpawner::RefillTick()
{
    TrySpawnOne();
}
