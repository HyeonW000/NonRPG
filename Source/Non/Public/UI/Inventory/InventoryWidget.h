#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/ItemEnums.h"
#include "InventoryWidget.generated.h"

class UTextBlock;
class UUniformGridPanel;
class UInventoryComponent;
class UEquipmentComponent;
class UInventorySlotWidget;
class UInventoryItem;

UCLASS()
class NON_API UInventoryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 인벤토리 컴포넌트를 연결(변경)합니다. 연결 시 슬롯을 재구성하고 델리게이트를 바인딩합니다. */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SetInventory(UInventoryComponent* InInventory);

    /** 생성할 슬롯 위젯 클래스(예: WBP_InventorySlot) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    TSubclassOf<UInventorySlotWidget> SlotWidgetClass;

    /** 슬롯을 채울 그리드 패널 (BP에서 이름을 'Grid'로 두면 자동 바인딩됨) */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory")
    UUniformGridPanel* Grid = nullptr;

    /** 골드 수치를 표시할 텍스트 블록 위젯 (BP에서 이름을 'GoldText'로 두면 자동 바인딩됨) */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory")
    UTextBlock* GoldText = nullptr;

    /** 컬럼 수 (열 개수). Row, Col = Index/Columns, Index%Columns */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 Columns = 8;

    // UIManager가 직접 호출할 초기화 함수
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void InitInventoryUI(UInventoryComponent* InInventory, UEquipmentComponent* InEquipment);

    /** [New] 특정 아이템 대분류 종류로 인벤토리 슬롯 필터링을 토글하는 함수 */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Filter")
    void ToggleFilter(EItemType FilterType);


protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
        UDragDropOperation* InOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
        UDragDropOperation* InOperation) override;

private:

    UPROPERTY() UInventoryComponent* OwnerInventory = nullptr;
    UPROPERTY() UEquipmentComponent* OwnerEquipment = nullptr;
    UPROPERTY() bool bInitOnce = false; // 필요시 중복 방지
    /** 현재 연결된 인벤토리 */
    UPROPERTY() TObjectPtr<UInventoryComponent> InvRef;

    /** 생성된 슬롯 위젯 캐시 */
    UPROPERTY() TArray<TObjectPtr<UInventorySlotWidget>> Slots;

    /** 슬롯 UI 구성 */
    void InitSlots();
    /** 기존 슬롯 UI 제거 */
    void ClearSlots();

    /** 전체 갱신 */
    void RefreshAll();

    /** 단일 슬롯 갱신(인벤토리 델리게이트 수신) */
    UFUNCTION()
    void HandleSlotUpdated(int32 Index, UInventoryItem* Item);

    /** 전체 리프레시(인벤토리 델리게이트 수신) */
    UFUNCTION()
    void HandleInventoryRefreshed();

    /** 델리게이트 바인딩/해제 유틸 */
    void BindDelegates();
    void UnbindDelegates();

    /** 골드 수치 변경 이벤트 처리 */
    UFUNCTION()
    void HandleGoldChanged(int32 NewGold);

    /** 골드 텍스트UI 최신화 */
    void UpdateGoldText();

    /** [New] 현재 설정된 필터를 모든 슬롯 위젯에 일괄 비주얼 적용 */
    void ApplyFilter();

    /** [New] 필터링이 활성화되어 있는지 여부 */
    UPROPERTY()
    bool bIsFilterActive = false;

    /** [New] 현재 켜져 있는 활성 필터 아이템 종류 */
    UPROPERTY()
    EItemType ActiveFilterType = EItemType::Etc;
};
