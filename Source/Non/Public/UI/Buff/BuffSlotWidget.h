#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BuffSlotWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * 🌟 버프 슬롯 위젯 (아이콘 + 남은 시간 텍스트)
 */
UCLASS()
class NON_API UBuffSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 버프 슬롯 초기화 */
    UFUNCTION(BlueprintCallable, Category = "Buff")
    void InitBuffSlot(FName InBuffId, const FText& InBuffName, UTexture2D* InIcon, float InDuration);

    /** 남은 시간 텍스트 갱신 */
    UFUNCTION(BlueprintCallable, Category = "Buff")
    void UpdateRemainingTime(float InRemaining);

    FName GetBuffId() const { return BuffId; }

    /** 버프 등장 시 블루프린트 커스텀 이벤트 (애니메이션, 파티클 등) */
    UFUNCTION(BlueprintImplementableEvent, Category = "Buff")
    void OnBuffAppear();

protected:
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* Image_BuffIcon = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* Text_RemainingTime = nullptr;

    /** 버프 이름 텍스트 (UMG에 배치 시 '분노' 등 이름 표시) */
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* Text_BuffName = nullptr;

    /** UMG 디자이너에서 'Anim_Appear' 애니메이션을 만들면 생성 시 자동 재생됩니다! */
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* Anim_Appear = nullptr;

private:
    FName BuffId;
    float TotalDuration = 0.f;
};
