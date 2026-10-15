#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatSlotComponent.generated.h"

class AEnemyCharacter;

/**
 * 개별 전투 포위 슬롯 정보
 */
USTRUCT(BlueprintType)
struct FCombatSlot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Slot")
    int32 SlotIndex = 0;

    /** 플레이어 정면 기준 각도 오프셋 (단위: 도, Degree) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Slot")
    float AngleOffsetDeg = 0.f;

    /** 현재 슬롯을 점유 중인 몬스터 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Slot")
    TWeakObjectPtr<AEnemyCharacter> ClaimedEnemy = nullptr;

    bool IsClaimed() const
    {
        return ClaimedEnemy.IsValid();
    }
};

/**
 * 플레이어 주변을 360도 원형 슬롯으로 분할하여,
 * 다수의 몬스터가 한 점으로 겹치지 않고 자연스럽게 둘러싸도록 관리하는 컴포넌트입니다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class NON_API UCombatSlotComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatSlotComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** 몬스터에게 최적의(가장 가까운) 비어있는 슬롯을 배정합니다. 배정된 슬롯 인덱스를 반환합니다. */
    UFUNCTION(BlueprintCallable, Category = "Combat|Slot")
    int32 ReserveSlot(AEnemyCharacter* Enemy);

    /** 몬스터가 점유하던 슬롯을 해제합니다. */
    UFUNCTION(BlueprintCallable, Category = "Combat|Slot")
    void ReleaseSlot(AEnemyCharacter* Enemy);

    /** 모든 슬롯의 점유를 일괄 해제합니다 (사망 시 등). */
    UFUNCTION(BlueprintCallable, Category = "Combat|Slot")
    void ReleaseAllSlots();

    /** 특정 몬스터가 배정받은 슬롯의 실시간 월드 좌표를 반환합니다. */
    UFUNCTION(BlueprintPure, Category = "Combat|Slot")
    FVector GetSlotLocation(const AEnemyCharacter* Enemy, float CustomRadius = -1.0f) const;

    /** 슬롯 인덱스 기반으로 실시간 월드 좌표를 반환합니다. */
    UFUNCTION(BlueprintPure, Category = "Combat|Slot")
    FVector GetSlotLocationByIndex(int32 SlotIndex, float CustomRadius = -1.0f) const;

    /** 몬스터가 이미 슬롯을 배정받았는지 검사합니다. */
    UFUNCTION(BlueprintPure, Category = "Combat|Slot")
    bool HasSlot(const AEnemyCharacter* Enemy) const;

    /** 전체 슬롯 개수 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Slot", meta = (ClampMin = "4", ClampMax = "16"))
    int32 NumSlots = 8;

    /** 기본 슬롯 반경 (cm 단위) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Slot")
    float DefaultSlotRadius = 140.0f;

    /** 에디터 및 게임 중 슬롯 위치 디버그 시각화 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Slot")
    bool bDebugDraw = false;

protected:
    void InitializeSlots();

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Slot")
    TArray<FCombatSlot> Slots;
};
