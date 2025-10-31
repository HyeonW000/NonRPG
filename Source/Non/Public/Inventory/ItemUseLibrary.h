#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemUseLibrary.generated.h"

class UInventoryComponent;

UCLASS()
class NON_API UItemUseLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // 인벤토리 슬롯의 소비 아이템 사용
    UFUNCTION(BlueprintCallable, Category = "Item|Use")
    static bool UseConsumable(AActor* InstigatorActor, UInventoryComponent* Inventory, int32 SlotIndex);

    // 슬롯의 아이템이 장비면 장착
    UFUNCTION(BlueprintCallable, Category = "Item")
    static bool UseOrEquip(AActor* InstigatorActor, UInventoryComponent* Inventory, int32 SlotIndex);
};
