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

    UFUNCTION(BlueprintCallable, BlueprintPure)
    USkillDataAsset* GetDataAsset() const
    {
        return DataAsset ? DataAsset : DefaultDataAsset;
    }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeOnInitialized() override;


    UFUNCTION() void OnSkillPointsChanged(int32 NewPoints);
    UFUNCTION() void OnSkillLevelChanged(FName SkillId, int32 NewLevel);
    UFUNCTION() void OnJobChanged(EJobClass NewJob);

    void BuildFromPlacedSlots();
    void Rebuild();
    void RefreshAll();

    UPROPERTY(meta = (BindWidget)) UTextBlock* Text_Points = nullptr;

    // 에디터에서 기본으로 넣어둘 수 있는 DA (직업 공통 테스트용)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
    USkillDataAsset* DefaultDataAsset = nullptr;

    // 실제 사용 중인 DA (직업별로 UIManager 가 넣어줌)
    UPROPERTY(BlueprintReadOnly, Category = "Skill")
    USkillDataAsset* DataAsset = nullptr;

    // 레벨/조건 확인용 매니저
    UPROPERTY()
    USkillManagerComponent* SkillMgr = nullptr;

    UPROPERTY() TMap<FName, USkillSlotWidget*> SlotMap;
    bool bBuilt = false;
};
