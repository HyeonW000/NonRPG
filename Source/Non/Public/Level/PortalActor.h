#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PortalActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ANonCharacterBase;

/**
 * 플레이어가 닿으면 데이터를 저장하고 다음 레벨 또는 인스턴스 던전으로 이동시키는 포탈 액터
 */
UCLASS(Blueprintable, BlueprintType)
class NON_API APortalActor : public AActor
{
    GENERATED_BODY()

public:
    APortalActor();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                        bool bFromSweep, const FHitResult& SweepResult);

    /** 세이브 및 실제 이동 실행 */
    void ExecuteTravel(ANonCharacterBase* PlayerChar);

public:
    /** 루트 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    /** 포탈 감지 충돌 박스 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    /** 포탈 시각 효과용 메쉬 (원하는 메쉬를 에디터에서 지정 가능) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Components")
    TObjectPtr<UStaticMeshComponent> PortalMesh;

    /** 이동할 목적지 레벨 이름 (예: LV_Town1, TestEnvironment, LV_StartMap) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Settings")
    FString TargetMapName = TEXT("LV_Town1");

    /** true일 경우 파티원들과 함께 인스턴스 던전으로 서버 이동(ServerTravel), false면 개인 로컬 이동 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Settings")
    bool bIsPartyDungeon = false;

    /** 이동 전 캐릭터 데이터를 자동으로 세이브할지 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Settings")
    bool bSaveBeforeTravel = true;

    /** 포탈에 닿은 후 이동까지의 지연 시간 (초) - 연출 및 안정성 확보 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Settings", meta = (ClampMin = "0.0"))
    float TravelDelay = 0.5f;

private:
    /** 중복 진입 방지 플래그 */
    bool bIsTraveling = false;
};
