#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DamageNumberWidget.generated.h"

class UTextBlock;
class UCanvasPanel;

UCLASS()
class NON_API UDamageNumberWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

    // 표시할 숫자/색 설정
    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void SetupNumber(float InValue, const FLinearColor& InColor, int32 InFontSize = 28);

    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void SetupLabel(const FText& InText, const FLinearColor& InColor, int32 InFontSize = 28);

    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void SetOutline(int32 InSize, FLinearColor InColor);


protected:
    // 코드로 생성한 텍스트
    UPROPERTY()
    TObjectPtr<UTextBlock> DamageText = nullptr;

    // 내부: 텍스트 구성
    void BuildIfNeeded();

    // 아웃라인 설정을 BP에서 바꿀 수 있게 노출
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DamageNumber|Appearance")
    int32 OutlineSize = 2;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DamageNumber|Appearance")
    FLinearColor OutlineColor = FLinearColor::Black;

private:
    float PendingValue = 0.f;
    FLinearColor PendingColor = FLinearColor::White;
    int32 PendingFontSize = 28;
    bool bBuilt = false;
};
