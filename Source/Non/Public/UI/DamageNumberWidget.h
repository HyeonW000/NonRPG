#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/NonDamageTypes.h"
#include "DamageNumberWidget.generated.h"

class UTextBlock;

UCLASS()
class NON_API UDamageNumberWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // 표시할 숫자 설정
    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void SetupNumber(float InValue, ENonDamageNumberCategory InCategory, int32 InFontSize = 28);

    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void SetupLabel(const FText& InText, ENonDamageNumberCategory InCategory, int32 InFontSize = 28);

    // 블루프린트(WBP)에서 비주얼을 커스텀하게 꾸밀 수 있도록 이벤트를 제공합니다.
    UFUNCTION(BlueprintImplementableEvent, Category = "DamageNumber")
    void OnSetupVisuals(float InValue, const FText& InText, ENonDamageNumberCategory InCategory);

protected:
    // UMG 디자이너에서 'DamageText'라는 이름으로 텍스트 위젯을 만들어야 합니다.
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "DamageNumber")
    TObjectPtr<UTextBlock> DamageText = nullptr;

    // 아웃라인 설정을 BP에서 바꿀 수 있게 노출
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DamageNumber|Appearance")
    int32 OutlineSize = 2;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DamageNumber|Appearance")
    FLinearColor OutlineColor = FLinearColor::Black;
};
