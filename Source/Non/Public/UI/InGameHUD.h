#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InGameHUD.generated.h"

class UInventoryWidget;
class UInventoryComponent;
class ANonCharacterBase;

UCLASS()
class NON_API UInGameHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void UpdateHP(float Current, float Max);

    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void UpdateMP(float Current, float Max);

    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void UpdateSP(float Current, float Max);

    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void UpdateEXP(float Current, float Max);

    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void UpdateLevel(int32 NewLevel);

};