#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHPBarWidget.generated.h"

class UProgressBar;
class UCanvasPanel;

UCLASS()
class NON_API UEnemyHPBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // HP 값 갱신 (Current/Max)
    UFUNCTION(BlueprintCallable, Category = "HPBar")
    void SetHP(float Current, float Max);

    // 색/스타일을 BP에서 바꾸고 싶을 때
    UFUNCTION(BlueprintCallable, Category = "HPBar")
    void SetBarColors(FLinearColor InFill, FLinearColor InBackground);

    virtual void NativeConstruct() override;

protected:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> Bar = nullptr;

    // 스타일 노출 (BP에서 기본값 수정 가능)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HPBar|Style")
    FLinearColor FillColor = FLinearColor(0.80f, 0.12f, 0.12f, 1.0f); // 적색

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HPBar|Style")
    FLinearColor BackgroundColor = FLinearColor(0.f, 0.f, 0.f, 0.40f); // 반투명 검정

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HPBar|Style", meta = (ClampMin = "40", ClampMax = "400"))
    int32 Width = 120;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HPBar|Style", meta = (ClampMin = "4", ClampMax = "40"))
    int32 Height = 12;

private:
    bool bBuilt = false;
    void BuildIfNeeded();
    void ApplyStyle();
};
