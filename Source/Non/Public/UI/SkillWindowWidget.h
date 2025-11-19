#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Skill/SkillTypes.h"
#include "SkillWindowWidget.generated.h"

class USkillManagerComponent;
class USkillDataAsset;
class USkillSlotWidget;
class UTextBlock;

UCLASS()
class USkillWindowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable)
    void Init(USkillManagerComponent* InMgr, USkillDataAsset* InData);

    UPROPERTY(EditAnywhere, Category = "Skill|Build")
    bool bUsePlacedSlots = true;

    // 헤더에서는 선언만 (구현은 .cpp)
    EJobClass GetJobClass() const;

    // 드롭다운에서 참조하므로 DataAsset 읽기 전용 게터 유지
    USkillDataAsset* GetDataAsset() const { return DataAsset; }

protected:
    virtual void NativeConstruct() override;

    UFUNCTION() void OnSkillPointsChanged(int32 NewPoints);
    UFUNCTION() void OnSkillLevelChanged(FName SkillId, int32 NewLevel);
    UFUNCTION() void OnJobChanged(EJobClass NewJob);

    void BuildFromPlacedSlots();
    void BuildAuto();
    void Rebuild();
    void RefreshAll();

    UPROPERTY(meta = (BindWidget)) UTextBlock* Text_Points = nullptr;

    UPROPERTY() USkillManagerComponent* SkillMgr = nullptr;
    UPROPERTY(EditAnywhere) USkillDataAsset* DataAsset = nullptr;

    UPROPERTY() TMap<FName, USkillSlotWidget*> SlotMap;
    bool bBuilt = false;
};
