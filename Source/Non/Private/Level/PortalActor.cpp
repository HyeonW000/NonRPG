#include "Level/PortalActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Character/NonCharacterBase.h"
#include "Core/NonPlayerController.h"
#include "System/SaveGameSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

APortalActor::APortalActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    // 1. 루트 컴포넌트
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    // 2. 포탈 오버랩 감지 박스 (기본 크기: 150x150x100)
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    TriggerBox->SetupAttachment(RootComponent);
    TriggerBox->SetBoxExtent(FVector(150.f, 150.f, 100.f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    TriggerBox->SetGenerateOverlapEvents(true);

    // 3. 포탈 외형 표시용 메쉬 (에디터에서 원하는 메쉬로 변경 가능)
    PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
    PortalMesh->SetupAttachment(RootComponent);
    PortalMesh->SetCollisionProfileName(TEXT("NoCollision"));
}

void APortalActor::BeginPlay()
{
    Super::BeginPlay();

    if (TriggerBox)
    {
        TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &APortalActor::OnOverlapBegin);
    }
}

void APortalActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                  bool bFromSweep, const FHitResult& SweepResult)
{
    if (bIsTraveling)
    {
        return;
    }

    ANonCharacterBase* PlayerChar = Cast<ANonCharacterBase>(OtherActor);
    if (!PlayerChar)
    {
        return;
    }

    APlayerController* PC = Cast<APlayerController>(PlayerChar->GetController());
    if (!PC)
    {
        return;
    }

    // 로컬 플레이어이거나 서버 권한이 있는 경우에만 포탈 트리거 반응
    if (!PC->IsLocalController() && !HasAuthority())
    {
        return;
    }

    bIsTraveling = true;

    if (TravelDelay > 0.0f)
    {
        FTimerHandle TravelTimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TravelTimerHandle, [this, PlayerChar]()
        {
            ExecuteTravel(PlayerChar);
        }, TravelDelay, false);
    }
    else
    {
        ExecuteTravel(PlayerChar);
    }
}

void APortalActor::ExecuteTravel(ANonCharacterBase* PlayerChar)
{
    if (TargetMapName.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[PortalActor] TargetMapName이 비어 있어 이동을 중단합니다."));
        bIsTraveling = false;
        return;
    }

    // 1. 이동 전 캐릭터 데이터 자동 세이브
    if (bSaveBeforeTravel)
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (USaveGameSubsystem* SaveSubsystem = GI->GetSubsystem<USaveGameSubsystem>())
            {
                SaveSubsystem->SaveGame();
                UE_LOG(LogTemp, Log, TEXT("[PortalActor] 레벨 이동 전 세이브 완료."));
            }
        }
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        bIsTraveling = false;
        return;
    }

    // 2. 이동 실행
    if (bIsPartyDungeon)
    {
        // ── 파티 인스턴스 던전 이동 (ServerTravel) ──
        if (HasAuthority())
        {
            FString TravelURL = TargetMapName;
            if (!TravelURL.StartsWith(TEXT("/Game/")))
            {
                TravelURL = FString::Printf(TEXT("/Game/Non/Maps/%s"), *TargetMapName);
            }
            TravelURL += TEXT("?listen");

            UE_LOG(LogTemp, Log, TEXT("[PortalActor] 파티 던전 ServerTravel 시작: %s"), *TravelURL);
            World->ServerTravel(TravelURL);
        }
        else if (PlayerChar)
        {
            // 클라이언트라면 서버에 맵 이동 요청 RPC
            if (ANonPlayerController* NonPC = Cast<ANonPlayerController>(PlayerChar->GetController()))
            {
                UE_LOG(LogTemp, Log, TEXT("[PortalActor] 서버에 파티 던전 이동 요청 RPC 전송: %s"), *TargetMapName);
                NonPC->ServerStartGame(TargetMapName);
            }
        }
    }
    else
    {
        // ── 개인 일반 맵 이동 (OpenLevel) ──
        UE_LOG(LogTemp, Log, TEXT("[PortalActor] 개인 맵 이동(OpenLevel) 시작: %s"), *TargetMapName);
        UGameplayStatics::OpenLevel(this, FName(*TargetMapName));
    }
}
