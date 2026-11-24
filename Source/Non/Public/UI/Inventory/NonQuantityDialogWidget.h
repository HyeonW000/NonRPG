#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NonQuantityDialogWidget.generated.h"

DECLARE_DELEGATE_OneParam(FOnQuantityConfirmed, int32);

UCLASS()
class NON_API UNonQuantityDialogWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 다이얼로그 초기 설정
    void SetupDialog(int32 InMaxQuantity, FOnQuantityConfirmed InOnConfirm);

protected:
    virtual void NativeConstruct() override;

private:
    UFUNCTION()
    void OnSliderValueChanged(float Value);

    UFUNCTION()
    void OnTextChanged(const FText& Text);

    UFUNCTION()
    void OnConfirmClicked();

    UFUNCTION()
    void OnCancelClicked();

    // 양방향 수량 동기화 및 텍스트/슬라이더 업데이트
    void UpdateQuantity(int32 NewQty, bool bUpdateSlider = true, bool bUpdateText = true);

    // UMG 생명주기 지연 반영을 위한 UI 새로고침 함수
    void RefreshDialogUI();

    int32 MaxQuantity = 1;
    int32 CurrentQuantity = 1;
    bool bIsInitialized = false;

    FOnQuantityConfirmed OnConfirm;

    // 위젯 포인터 캐싱 (런타임 GetWidgetFromName으로 찾음).
    // 멤버 이름을 WBP 위젯 이름(Slider_Quantity 등)과 다르게 둬야 WBP가 만드는 같은 이름 변수와 충돌하지 않음
    UPROPERTY(Transient)
    TObjectPtr<class USlider> QuantitySlider = nullptr;
    UPROPERTY(Transient)
    TObjectPtr<class UEditableTextBox> QuantityTextBox = nullptr;
    UPROPERTY(Transient)
    TObjectPtr<class UEditableText> QuantityTextBasic = nullptr;
    UPROPERTY(Transient)
    TObjectPtr<class UButton> ConfirmButton = nullptr;
    UPROPERTY(Transient)
    TObjectPtr<class UButton> CancelButton = nullptr;
    UPROPERTY(Transient)
    TObjectPtr<class UTextBlock> MaxQuantityText = nullptr;
};
