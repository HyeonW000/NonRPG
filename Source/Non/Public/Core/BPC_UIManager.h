#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "TimerManager.h"
#include "BPC_UIManager.generated.h"

class UInGameHUD;
class UUserWidget;
class UInventoryComponent;
class UEquipmentComponent;
class UCharacterWindowWidget;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class NON_API UBPC_UIManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UBPC_UIManager();
    virtual void BeginPlay() override;

    // ----- HUD -----
    void InitHUD();
    UInGameHUD* GetInGameHUD() const { return InGameHUD; }

    // ----- HUD bridge updates -----
    void UpdateHP(float Current, float Max);
    void UpdateMP(float Current, float Max);
    void UpdateSP(float Current, float Max);
    void UpdateEXP(float Current, float Max);
    void UpdateLevel(int32 NewLevel);

    // ----- Inventory Window -----
    UFUNCTION(BlueprintCallable) void ToggleInventory();
    UFUNCTION(BlueprintCallable) void ShowInventory();
    UFUNCTION(BlueprintCallable) void HideInventory();
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Inventory")
    bool IsInventoryVisible() const;
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Inventory")
    UUserWidget* GetInventoryWidget() const;

    // ----- Character Window -----
    UFUNCTION(BlueprintCallable) void ToggleCharacter();
    UFUNCTION(BlueprintCallable) void ShowCharacter();
    UFUNCTION(BlueprintCallable) void HideCharacter();
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Character")
    bool IsCharacterVisible() const;
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Character")
    UUserWidget* GetCharacterWidget() const;

    // ----- Common Window Ops -----
    UFUNCTION(BlueprintCallable, Category = "UI|Windows")
    void RegisterWindow(UUserWidget* Window);
    UFUNCTION(BlueprintCallable, Category = "UI|Windows")
    void UnregisterWindow(UUserWidget* Window);
    UFUNCTION(BlueprintCallable, Category = "UI|Windows")
    void BringToFront(UUserWidget* Window);
    UFUNCTION(BlueprintCallable, Category = "UI|Windows")
    void NotifyWindowClosed(UUserWidget* Window);

    // 지연 좌표 적용 큐
    UFUNCTION(BlueprintCallable)
    void QueueSetViewportPosition(UUserWidget* Window, const FVector2D& ViewportPos);

    // 외부 호환용
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Windows")
    bool IsAnyWindowOpen() const { return IsAnyWindowVisible(); }

    // Hover 추적
    void NotifyWindowHover(UUserWidget* Window, bool bHovered)
    {
        if (bHovered) ++HoveredWindowCount;
        else          HoveredWindowCount = FMath::Max(0, HoveredWindowCount - 1);
    }
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Windows")
    bool IsCursorOverUI() const { return HoveredWindowCount > 0; }

    UFUNCTION(BlueprintCallable, Category = "UI")
    void RefreshCharacterEquipmentUI();

protected:
    void SetUIInputMode(bool bShowCursor = false);
    void SetGameInputMode();
    bool IsAnyWindowVisible() const;

    // 위젯 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|HUD")
    TSubclassOf<UInGameHUD> InGameHUDClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Inventory")
    TSubclassOf<UUserWidget> InventoryWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Character")
    TSubclassOf<UUserWidget> CharacterWidgetClass;

    // 인벤토리 기본 좌표
    UPROPERTY(EditAnywhere, Category = "UI|Inventory")
    FVector2D InventoryDefaultPos = FVector2D(1200.f, 300.f);

private:
    APlayerController* GetPC() const;
    void ApplyQueuedViewportPositions();
    static bool GetWidgetViewportPos(UUserWidget* W, FVector2D& OutViewportPos);

private:
    bool bUIOpen = false;
    UPROPERTY() UInGameHUD* InGameHUD = nullptr;
    UPROPERTY() TWeakObjectPtr<UUserWidget> InventoryWidget;
    UPROPERTY() TWeakObjectPtr<UUserWidget> CharacterWidget;
    UPROPERTY() bool bInventoryUIInited = false;

    UPROPERTY() TArray<TWeakObjectPtr<UUserWidget>> OpenWindows;
    int32 TopZOrder = 1000;

    TMap<TWeakObjectPtr<UUserWidget>, FVector2D> DesiredViewportPositions;
    FTimerHandle DeferredPosTimerHandle;
    bool bDeferredPosScheduled = false;

    TMap<TWeakObjectPtr<UUserWidget>, FVector2D> SavedViewportPositions;

    int32 HoveredWindowCount = 0;
};
