#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuickSlotSlotWidget.generated.h"

class UQuickSlotManager;
class UInventoryComponent;
class UInventoryItem;
class UItemDragDropOperation;
class UImage;
class UTextBlock;

UCLASS()
class NON_API UQuickSlotSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot", meta = (ClampMin = "0", ClampMax = "9"))
    int32 QuickIndex = 0;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "QuickSlot|UI")
    UImage* IconImage = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "QuickSlot|UI")
    UTextBlock* CountText = nullptr;

    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "QuickSlot")
    void UpdateVisualBP(UInventoryItem* Item);

    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    void UpdateVisual(UInventoryItem* Item);

    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    void Refresh();

    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    void SetManager(UQuickSlotManager* InManager);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot")
    TSubclassOf<UUserWidget> DragVisualClass = nullptr;

protected:
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void   NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool   NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool   NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
    TWeakObjectPtr<UQuickSlotManager> Manager;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> CachedIcon = nullptr;
};
