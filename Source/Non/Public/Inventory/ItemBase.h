#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/ItemStructs.h"
#include "ItemBase.generated.h"

UCLASS(BlueprintType, Blueprintable)
class NON_API UItemBase : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    FName ItemId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    TObjectPtr<UDataTable> ItemDataTable = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
    FItemRow ItemData;

    UFUNCTION(BlueprintCallable, Category = "Item")
    bool LoadFromDataTable();

    UFUNCTION(BlueprintPure, Category = "Item")
    const FItemRow& GetRow() const { return ItemData; }
};
