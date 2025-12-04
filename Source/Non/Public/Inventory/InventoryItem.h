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
