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

    // ------- 입력/드래그 오버라이드 -------
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

    // ★ 더블클릭 → 아이템 사용
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;


    // ------- 외부 바인딩 대상 -------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Drag")
    TSubclassOf<UUserWidget> DragVisualClass = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    TObjectPtr<UInventoryComponent> OwnerInventory = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TObjectPtr<UInventoryItem> Item = nullptr;

    // === UI 갱신 ===
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void RefreshFromInventory();

    // 쿨다운(남은 시간/총 시간) 업데이트. 남은 시간이 0 이하이면 숨김.
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void UpdateCooldown(float Remaining, float Duration);

    // 쿨다운 표시용 MID (런타임 생성)
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

    // 아이콘/수량 갱신
    void UpdateVisual();

    // 좌클릭 다운에서 드래그 무장 상태
    bool bDragArmed = false;

    // ★ 아이템 드래그 진행 상태(슬롯이 시작한 드래그인지 추적)
    bool bDraggingItemActive = false;   // <-- 추가

    // ★ 내부 헬퍼: 이 슬롯의 아이템 사용 시도
    void TryUseThisItem();

    // WASD 끊김 방지용: 게임 포커스 유지 헬퍼
    void KeepGameInputFocus();

    // 더블클릭 판정/드래그 지연에 필요
    double   LastClickTime = 0.0;
    FVector2D LastClickPosSS = FVector2D::ZeroVector;
    FVector2D MouseDownStartSS = FVector2D::ZeroVector;
    bool bLeftPressed = false;

    // 튜닝값(원하면 에디터에서 조절)
    UPROPERTY(EditAnywhere, Category = "Inventory|Input")
    float DoubleClickSeconds = 0.40f;     // 0.4초로 여유있게

    UPROPERTY(EditAnywhere, Category = "Inventory|Input")
    float DragSlopPixels = 12.f;          // DPI 스케일 적용 전 기준 픽셀
};
