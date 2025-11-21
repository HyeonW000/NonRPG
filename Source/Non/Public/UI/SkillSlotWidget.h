#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Skill/SkillTypes.h"
#include "SkillSlotWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UBorder;
class USizeBox;
class USkillManagerComponent;
class USkillDataAsset;

/** 단일 스킬 슬롯 (에디터 배치형) */
UCLASS()
class USkillSlotWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    /** 슬롯 데이터/매니저 주입 */
    void SetupSlot(const FSkillRow& InRow, USkillManagerComponent* InMgr);

    /** 표시 갱신 */
    UFUNCTION(BlueprintCallable) void Refresh();

    /** 에디터에서 선택할 스킬 ID (드롭다운) */
    UPROPERTY(EditInstanceOnly, Category = "Skill", meta = (GetOptions = "GetSkillIdOptions"))
    FName SkillId;

    /** (옵션) 이 슬롯만 다른 DataAsset 사용 */
    UPROPERTY(EditInstanceOnly, Category = "Skill")
    USkillDataAsset* SkillDataOverride = nullptr;

protected:
    virtual void NativeOnInitialized() override;

    /** 드롭다운 옵션 제공 */
    UFUNCTION() TArray<FName> GetSkillIdOptions() const;

    UFUNCTION() void OnClicked_LevelUp();
    UFUNCTION() void OnIconLoaded();

    // ===== 바인딩 =====
    UPROPERTY(meta = (BindWidget)) UTextBlock* Text_Level = nullptr;
    UPROPERTY(meta = (BindWidget)) UButton* Btn_LevelUp = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) UBorder* LockOverlay = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) UImage* IconImage = nullptr;

    // ===== 데이터 =====
    UPROPERTY() USkillManagerComponent* SkillMgr = nullptr;
    UPROPERTY() FSkillRow Row;
};
