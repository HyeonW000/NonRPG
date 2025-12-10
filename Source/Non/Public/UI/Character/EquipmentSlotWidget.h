#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/ItemEnums.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "EquipmentSlotWidget.generated.h"

class UEquipmentComponent;
class UInventoryItem;
class UImage;
class UBorder;

UCLASS()
class NON_API UEquipmentSlotWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
    EEquipmentSlot SlotType = EEquipmentSlot::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
    TObjectPtr<UEquipmentComponent> OwnerEquipment = nullptr;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>  IconImage;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> HighlightBorder;

    // 미러(고스트) 상태
    bool bIsMirror = false;
    EEquipmentSlot MirrorFrom = EEquipmentSlot::None;

    // 외부 호출
    void UpdateVisual(UInventoryItem* Item);
    void SetMirrorFrom(EEquipmentSlot From, UInventoryItem* Item);
    void ClearMirror();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror")
    EEquipmentSlot MirrorSource = EEquipmentSlot::None;

    UPROPERTY()
    TObjectPtr<UTexture2D> CachedIcon = nullptr;

    UFUNCTION(BlueprintCallable, Category = "EquipmentSlot")
    void SetOwnerEquipment(UEquipmentComponent* InEq) { OwnerEquipment = InEq; BindEquipmentDelegates(); }


    UFUNCTION()
    void RefreshFromComponent(); // 표시용 리프레시: 고스트 포함

protected:

    // === 추가: 델리게이트 바인딩/해제 & 핸들러 ===
    void BindEquipmentDelegates();
    void UnbindEquipmentDelegates();

    //  드롭 허용 판정 (슬롯 호환/양손 무기 제한 포함)
    bool CanAcceptItem(const UInventoryItem* Item) const;

    //  하이라이트(허용/불가 색상)
    void SetHighlight(bool bShow, bool bAllowed);

    UPROPERTY(Transient)
    bool bMirrorGhost = false;

    void ApplyGhost(bool bOn);

    UPROPERTY(EditDefaultsOnly, Category = "DragDrop")
    TSubclassOf<UUserWidget> DragVisualClass;

    UFUNCTION()
    void HandleEquipped(EEquipmentSlot InEquipSlot, UInventoryItem* Item);

    UFUNCTION()
    void HandleUnequipped(EEquipmentSlot InEquipSlot);

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;


    virtual void NativeOnDragEnter(const FGeometry& Geo, const FDragDropEvent& Ev, UDragDropOperation* Op) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& Ev, UDragDropOperation* Op) override;
    virtual bool NativeOnDragOver(const FGeometry& Geo, const FDragDropEvent& Ev, UDragDropOperation* Op) override;
    virtual bool NativeOnDrop(const FGeometry& Geo, const FDragDropEvent& Ev, UDragDropOperation* Op) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

};