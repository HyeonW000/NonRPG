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

/**
 * QuickSlot의 한 칸을 담당하는 슬롯 위젯
 * - 인벤토리에서 드래그 → 이 슬롯에 드랍하여 지정
 * - Manager로부터 ResolveItem(QuickIndex) 해서 표시 갱신
 * - 아이콘/수량은 C++ 기본 로직(UpdateVisual)로 처리 + BP에서 추가 연출은 UpdateVisualBP에서
 */
UCLASS()
class NON_API UQuickSlotSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 이 슬롯의 빠른슬롯 인덱스 (0~9) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot", meta = (ClampMin = "0", ClampMax = "9"))
    int32 QuickIndex = 0;

    /** (옵션) 디자이너에서 Icon, Count 텍스트를 바인딩해두면 C++에서 자동 갱신 */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "QuickSlot|UI")
    UImage* IconImage = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "QuickSlot|UI")
    UTextBlock* CountText = nullptr;

    /** BP에서 추가 연출/스킨 적용용 (C++ 기본 처리 후 마지막에 호출됨) */
    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "QuickSlot")
    void UpdateVisualBP(UInventoryItem* Item);

    /** 외부(바 위젯)에서 호출하는 공개 갱신 함수 */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    void UpdateVisual(UInventoryItem* Item);

    /** Manager.ResolveItem(QuickIndex)로 아이템 찾아와 표시 갱신 */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    void Refresh();

    /** 바 위젯(C++)에서 매니저 주입 */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    void SetManager(UQuickSlotManager* InManager);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot")
    TSubclassOf<UUserWidget> DragVisualClass = nullptr;

protected:
    // Drag&Drop 허용
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void   NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
    /** 소유 매니저 (바 위젯에서 주입됨) */
    TWeakObjectPtr<UQuickSlotManager> Manager;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> CachedIcon = nullptr; // 마지막으로 보여준 아이콘
};
