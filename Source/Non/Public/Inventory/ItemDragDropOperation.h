#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Data/ItemStructs.h"
#include "Inventory/ItemEnums.h"
#include "ItemDragDropOperation.generated.h"

class UInventoryItem;
class UInventoryComponent;
class UEquipmentComponent;
class UUserWidget;
class SViewport;
class UQuickSlotManager;

UCLASS()
class NON_API UItemDragDropOperation : public UDragDropOperation
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadWrite, Category = "DragDrop")
    TObjectPtr<UInventoryItem> Item = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "DragDrop")
    UInventoryComponent* SourceInventory = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "DragDrop")
    bool bFromQuickSlot = false;

    UPROPERTY(BlueprintReadWrite, Category = "DragDrop")
    int32 SourceQuickIndex = INDEX_NONE;

    UPROPERTY() TWeakObjectPtr<UQuickSlotManager> SourceQuickManager;

    UPROPERTY(BlueprintReadWrite, Category = "DragDrop")
    int32 SourceIndex = INDEX_NONE;

    //  추가: 장비슬롯에서 시작된 드래그인지
    UPROPERTY(BlueprintReadWrite, Category = "DragDrop")
    bool bFromEquipment = false;

    //  추가: 어느 장비 슬롯에서 나왔는지
    UPROPERTY(BlueprintReadWrite, Category = "DragDrop")
    EEquipmentSlot FromEquipSlot = EEquipmentSlot::None;

    // (선택) BP에서 쓸 수도 있으니 유지
    UPROPERTY(BlueprintReadWrite, Category = "DragDrop")
    TObjectPtr<UUserWidget> DragVisual = nullptr;

    virtual void Dragged_Implementation(const FPointerEvent& PointerEvent) override;
    virtual void Drop_Implementation(const FPointerEvent& PointerEvent) override;
    virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;

private:
    static TSharedPtr<SViewport> GetGameViewportSViewport(UWorld* World);
    static void KeepGameFocus(UObject* WorldContext);
};
