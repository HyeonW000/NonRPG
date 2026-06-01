#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/ItemEnums.h"
#include "ItemToolTipWidget.generated.h"

class UTextBlock;
class URichTextBlock;
class UImage;
class UBorder;
class UInventoryItem;

UCLASS()
class NON_API UItemToolTipWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 아이템 데이터를 기반으로 툴팁 텍스트 및 등급 색상을 주입합니다. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|ToolTip")
    void SetItemData(UInventoryItem* InItem);

    /** 툴팁에 연결될 UI 위젯 바인딩 변수들 (블루프린트에서 이름만 맞춰주면 자동 조립!) */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UTextBlock* TxtItemName = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UTextBlock* TxtItemRarity = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UTextBlock* TxtPrice = nullptr;

    /** [New] 아이템의 대표 외형을 보여줄 아이콘 이미지 위젯 */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UImage* ImgIcon = nullptr;

    /** [New] 장비 아이템일 경우 세부 물리/마법 능력치들을 보여줄 텍스트 위젯 */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    URichTextBlock* TxtMainStats = nullptr;

    /** [New UI UX] 2세트~6세트 효과를 가변적으로 디자이너가 직접 배치할 수 있도록 확장한 텍스트 블록 */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UTextBlock* TxtSet2Effect = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UTextBlock* TxtSet3Effect = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UTextBlock* TxtSet4Effect = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UTextBlock* TxtSet5Effect = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UTextBlock* TxtSet6Effect = nullptr;

    /** [New] 세트 효과가 활성화되었을 때 에디터에서 지정할 눈부신 색상 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|ToolTipUI")
    FLinearColor ActiveSetColor = FLinearColor(0.2f, 0.95f, 0.5f, 1.0f);

    /** [New] 세트 효과가 비활성화(미획득) 상태일 때 에디터에서 지정할 어두운 색상 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|ToolTipUI")
    FLinearColor InactiveSetColor = FLinearColor(0.45f, 0.45f, 0.45f, 1.0f);

    /** [New] 장비 비교 시 능력치가 상승했을 때의 에디터 지정 색상 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|ToolTipUI")
    FLinearColor IncreaseStatColor = FLinearColor(0.2f, 0.95f, 0.5f, 1.0f);

    /** [New] 장비 비교 시 능력치가 하강했을 때의 에디터 지정 색상 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|ToolTipUI")
    FLinearColor DecreaseStatColor = FLinearColor(0.95f, 0.2f, 0.2f, 1.0f);

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UBorder* BorderOutline = nullptr;

    /** [New] 장착 중인 비교 장비 상세 정보를 2중 팝업 패널로 나란히 띄울 서브 위젯 */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UItemToolTipWidget* CompareToolTipWidget = nullptr;

    /** [New] 세트 장비일 때 세트 명칭을 단독으로 노출해 가독성을 높일 전용 텍스트 위젯 */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UTextBlock* TxtSetName = nullptr;

    /** [New] 자신이 장착 중인 비교 템임을 유저가 자유롭게 배치하여 알릴 독립 텍스트 위젯 */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UTextBlock* TxtEquipped = nullptr;

    /** [New] 아이콘 이미지 주변의 등급별 다이내믹 프레임을 칠해줄 보더 위젯 */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UBorder* BorderIconFrame = nullptr;

    /** [New] 툴팁 가장 밑바닥에 디자이너가 깔아둘 배경 보더 위젯 */
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|ToolTip")
    UBorder* BorderBackground = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|ToolTipUI")
    TMap<EItemRarity, FLinearColor> RarityColors;


    /** [New] 자신이 비교용 서브 팝업 패널인지 여부 (재귀 무한 루프 차단 안전 제어 장치) */
    UPROPERTY(BlueprintReadWrite, Category = "Inventory|ToolTip")
    bool bIsComparePanel = false;
};
