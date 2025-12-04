#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventorySlotWidget.generated.h"

class UUserWidget;
class UInventoryItem;
class UInventoryComponent;
class UImage;
class UTextBlock;
class UBorder;
class UMaterialInstanceDynamic;

UCLASS()
class NON_API UInventorySlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void InitSlot(UInventoryComponent* InInventory, int32 InIndex);

    UFUNCTION()
    void HandleSlotUpdated(int32 UpdatedIndex, UInventoryItem* UpdatedItem);

    UFUNCTION()
    void HandleInventoryRefreshed();

    UInventorySlotWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeOnInitialized() override;

    virtual void   NativeConstruct() override;
    virtual void   NativeDestruct() override;
    virtual void   NativeTick(const FGeometry& G, float InDeltaTime) override;
    virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& G, const FPointerEvent& E) override;
    virtual FReply NativeOnMouseMove(const FGeometry& G, const FPointerEvent& E) override;
    virtual void   NativeOnDragDetected(const FGeometry& G, const FPointerEvent& E, UDragDropOperation*& OutOp) override;
    virtual void   NativeOnDragCancelled(const FDragDropEvent& DragDropEvent, UDragDropOperation* InOp) override;
    virtual bool   NativeOnDragOver(const FGeometry& G, const FDragDropEvent& DragDropEvent, UDragDropOperation* InOp) override;
    virtual bool   NativeOnDrop(const FGeometry& G, const FDragDropEvent& DragDropEvent, UDragDropOperation* InOp) override;

    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drag")
    TSubclassOf<UUserWidget> DragVisualClass = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    TObjectPtr<UInventoryComponent> OwnerInventory = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TObjectPtr<UInventoryItem> Item = nullptr;

    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void RefreshFromInventory();

    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void UpdateCooldown(float Remaining, float Duration);

    UPROPERTY() UMaterialInstanceDynamic* CooldownMID = nullptr;

    UPROPERTY(meta = (BindWidgetOptional)) UImage* ImgIcon = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TxtCount = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) UImage* ImgCooldownRadial = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) UBorder* BorderSlot = nullptr;

    UFUNCTION()
    FEventReply OnBorderMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

    UFUNCTION()
    FEventReply OnBorderMouseUp(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

    UFUNCTION()
    FEventReply OnBorderMouseMove(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

private:
    bool bDelegatesBound = false;

    void UpdateVisual();

    bool bDragArmed = false;
    bool bDraggingItemActive = false;

    void TryUseThisItem();
    void KeepGameInputFocus();

    double   LastClickTime = 0.0;
    FVector2D LastClickPosSS = FVector2D::ZeroVector;
    FVector2D MouseDownStartSS = FVector2D::ZeroVector;
    bool bLeftPressed = false;

    UPROPERTY(EditAnywhere, Category = "Inventory|Input")
    float DoubleClickSeconds = 0.40f;

    UPROPERTY(EditAnywhere, Category = "Inventory|Input")
    float DragSlopPixels = 12.f;
};
