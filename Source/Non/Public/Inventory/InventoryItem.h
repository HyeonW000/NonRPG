#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/ItemStructs.h"
#include "InventoryItem.generated.h"

UCLASS(BlueprintType)
class NON_API UInventoryItem : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    FName ItemId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
    FGuid InstanceId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    int32 Quantity = 1;

    // [New] 동적으로 저장되는 강화 수치 (데이터테이블이 아닌 인벤토리 객체에서 들고 있음)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    int32 EnhancementLevel = 0;

    // 강화 천장 게이지 (0~1). 실패할 때 차고 가득 차면 다음 강화 확정 성공 (NON-DIS-1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    float EnhanceGauge = 0.f;

    /** 세이브·복제에서 받은 개별 아이템 값 적용 (고유 ID가 비어 있으면 Init에서 만든 것을 유지) */
    void ApplyInstanceData(const FGuid& InInstanceId, int32 InEnhancementLevel, float InEnhanceGauge)
    {
        if (InInstanceId.IsValid())
        {
            InstanceId = InInstanceId;
        }
        EnhancementLevel = FMath::Max(0, InEnhancementLevel);
        EnhanceGauge = FMath::Clamp(InEnhanceGauge, 0.f, 1.f);
    }

    // [New] 신규 획득 아이템 알람 점등 여부 플래그
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    bool bIsNewItem = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
    FItemRow CachedRow;

    UPROPERTY(Transient)
    TObjectPtr<UDataTable> ItemDataTable;

    UFUNCTION(BlueprintCallable) void Init(FName InItemId, int32 InQty, UDataTable* DT);
    UFUNCTION(BlueprintPure)   bool IsStackable() const { return CachedRow.MaxStack > 1; }

    UFUNCTION(BlueprintPure, Category = "Item")
    int32 GetStackCount() const { return Quantity; }

    UFUNCTION(BlueprintPure, Category = "Item")
    int32 GetMaxStack() const { return CachedRow.MaxStack; }

    UFUNCTION(BlueprintPure, Category = "Item")
    UTexture2D* GetIcon() const
    {
        return CachedRow.Icon.IsNull() ? nullptr : CachedRow.Icon.LoadSynchronous();
    }

    //  슬롯 판정용 접근자 추가
    EEquipmentSlot GetEquipSlot() const;

    UFUNCTION(BlueprintPure, Category = "Item")
    bool IsEquipment() const;

    bool IsTwoHandedWeapon() const;

    UFUNCTION(BlueprintPure, Category = "Item")
    FName GetAttachSocket() const
    {
        // CachedRow.AttachSocket 가 NONE이면 데이터테이블에 빈 값입니다.
        return CachedRow.AttachSocket;
    }
};
