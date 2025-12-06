#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NonInteractableInterface.h"
#include "NonItemPickupBase.generated.h"

class USphereComponent;
class USkeletalMeshComponent;
class ANonCharacterBase;

UCLASS()
class NON_API ANonItemPickupBase : public AActor, public INonInteractableInterface
{
    GENERATED_BODY()

public:
    ANonItemPickupBase();

protected:
    // 상호작용용 콜리전 (TraceChannel "Interact" 만 Block)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
    USphereComponent* InteractCollision;

    // 보여줄 무기 메쉬 (Skeletal 기준, Static이면 나중에 바꿔도 됨)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
    USkeletalMeshComponent* ItemMesh;

    // 인벤토리/데이터테이블에서 쓰는 RowName (== ItemId)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
    FName ItemId = NAME_None;

    // 개수
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
    int32 Count = 1;

public:
    // 인터페이스 구현
    virtual FText GetInteractLabel_Implementation() override;
    virtual void Interact_Implementation(ANonCharacterBase* Interactor) override;
    virtual void SetInteractHighlight_Implementation(bool bEnable) override;

    // BP에서 세팅 편하게
    UFUNCTION(BlueprintCallable, Category = "Pickup")
    void SetItemData(FName InItemId, int32 InCount)
    {
        ItemId = InItemId;
        Count = InCount;
    }
};
