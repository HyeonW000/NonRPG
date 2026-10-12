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
    if (TargetMapName.IsEmpty() || !PlayerChar)
    {
        UE_LOG(LogTemp, Warning, TEXT("[PortalActor] TargetMapName 또는 PlayerChar가 유효하지 않아 이동을 중단합니다."));
        bIsTraveling = false;
        return;
    }

    ANonPlayerController* NonPC = Cast<ANonPlayerController>(PlayerChar->GetController());
    if (!NonPC)
    {
        bIsTraveling = false;
        return;
    }

    // 파티원 한정 자동 동반 이동 및 비파티원 오픈월드 잔류 처리 RPC 호출
    UE_LOG(LogTemp, Log, TEXT("[PortalActor] 던전 이동 요청(Server_RequestPartyDungeonTravel): %s"), *TargetMapName);
    NonPC->Server_RequestPartyDungeonTravel(TargetMapName);

    // 중복 연타 방지: 2초 후 플래그 리셋
    FTimerHandle ResetTimerHandle;
    GetWorld()->GetTimerManager().SetTimer(ResetTimerHandle, [this]()
    {
        bIsTraveling = false;
    }, 2.0f, false);
}
