#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillTypes.h"
#include "Components/ActorComponent.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "TimerManager.h"
#include "BPC_UIManager.generated.h"

class UInGameHUD;
class UUserWidget;
class UInventoryComponent;
class UEquipmentComponent;
class UCharacterWindowWidget;
class USkillWindowWidget;

UCLASS(ClassGroup = (UI), meta = (BlueprintSpawnableComponent))
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

    //Skill Window
    UFUNCTION(BlueprintCallable) void ToggleSkillWindow();
    UFUNCTION(BlueprintCallable) void ShowSkillWindow();
    UFUNCTION(BlueprintCallable) void HideSkillWindow();

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

    // 에디터에서 WBP_SkillWindow 지정
    UPROPERTY(EditDefaultsOnly, Category = "UI|Skill")
    TSubclassOf<UUserWidget> SkillWindowClass = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> SkillWindow = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Skill")
    TMap<EJobClass, USkillDataAsset*> SkillDataByJob;

private:
    APlayerController* GetPC() const;
    static bool GetWidgetViewportPos(UUserWidget* W, FVector2D& OutViewportPos);

    bool bUIOpen = false;
    UPROPERTY() UInGameHUD* InGameHUD = nullptr;
    UPROPERTY() TWeakObjectPtr<UUserWidget> InventoryWidget;
    UPROPERTY() TWeakObjectPtr<UUserWidget> CharacterWidget;
    UPROPERTY() bool bInventoryUIInited = false;

    UPROPERTY() TArray<TWeakObjectPtr<UUserWidget>> OpenWindows;
    int32 TopZOrder = 1000;

    FTimerHandle DeferredPosTimerHandle;
    bool bDeferredPosScheduled = false;


    int32 HoveredWindowCount = 0;

    static FVector2D ClampToViewport(const FVector2D& InPx, const FVector2D& ViewportPx, const FVector2D& WindowPx)
    {
        const float X = FMath::Clamp(InPx.X, 0.f, FMath::Max(0.f, ViewportPx.X - WindowPx.X));
        const float Y = FMath::Clamp(InPx.Y, 0.f, FMath::Max(0.f, ViewportPx.Y - WindowPx.Y));
        return FVector2D(X, Y);
    }
};
