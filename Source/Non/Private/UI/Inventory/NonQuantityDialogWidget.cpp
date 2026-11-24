#include "UI/Inventory/NonQuantityDialogWidget.h"
#include "Components/Slider.h"
#include "Components/EditableTextBox.h"
#include "Components/EditableText.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

void UNonQuantityDialogWidget::SetupDialog(int32 InMaxQuantity, FOnQuantityConfirmed InOnConfirm)
{
    MaxQuantity = FMath::Max(1, InMaxQuantity);
    OnConfirm = InOnConfirm;
    bIsInitialized = true;

    // 만약 NativeConstruct가 SetupDialog보다 먼저 실행된 상태(드물지만 안전장치)라면 즉시 반영
    if (QuantitySlider || MaxQuantityText || QuantityTextBox || QuantityTextBasic)
    {
        RefreshDialogUI();
    }
}

void UNonQuantityDialogWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 블루프린트 위젯 내의 이름 매핑 (UI 레이아웃과의 컴파일 디펜던시 완전 차단)
    QuantitySlider = Cast<USlider>(GetWidgetFromName(TEXT("Slider_Quantity")));
    
    // UEditableTextBox 와 UEditableText 둘 다 유연하게 대응
    UWidget* FoundTextWidget = GetWidgetFromName(TEXT("EditableText_Quantity"));
    QuantityTextBox = Cast<UEditableTextBox>(FoundTextWidget);
    QuantityTextBasic = Cast<UEditableText>(FoundTextWidget);

    ConfirmButton = Cast<UButton>(GetWidgetFromName(TEXT("Button_Confirm")));
    CancelButton = Cast<UButton>(GetWidgetFromName(TEXT("Button_Cancel")));
    MaxQuantityText = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_MaxQuantity")));

    // 필수 위젯 바인딩이 빠졌을 때만 경고 (이름 대소문자, IsVariable 확인)
    const bool bHasTextInput = (QuantityTextBox != nullptr) || (QuantityTextBasic != nullptr);
    if (!QuantitySlider || !bHasTextInput || !ConfirmButton || !CancelButton)
    {
        UE_LOG(LogTemp, Warning, TEXT("[QuantityDialog] 위젯 바인딩 누락: Slider=%d Text=%d Confirm=%d Cancel=%d"),
            QuantitySlider ? 1 : 0, bHasTextInput ? 1 : 0, ConfirmButton ? 1 : 0, CancelButton ? 1 : 0);
    }

    if (QuantitySlider)
    {
        QuantitySlider->OnValueChanged.AddDynamic(this, &UNonQuantityDialogWidget::OnSliderValueChanged);
    }

    if (QuantityTextBox)
    {
        QuantityTextBox->OnTextChanged.AddDynamic(this, &UNonQuantityDialogWidget::OnTextChanged);
    }
    else if (QuantityTextBasic)
    {
        QuantityTextBasic->OnTextChanged.AddDynamic(this, &UNonQuantityDialogWidget::OnTextChanged);
    }

    if (ConfirmButton)
    {
        ConfirmButton->OnClicked.AddDynamic(this, &UNonQuantityDialogWidget::OnConfirmClicked);
    }

    if (CancelButton)
    {
        CancelButton->OnClicked.AddDynamic(this, &UNonQuantityDialogWidget::OnCancelClicked);
    }

    // 만약 SetupDialog가 먼저 호출되어 변수가 준비되어 있다면 UI 세팅을 갱신합니다.
    if (bIsInitialized)
    {
        RefreshDialogUI();
    }
}

void UNonQuantityDialogWidget::RefreshDialogUI()
{
    if (MaxQuantityText)
    {
        MaxQuantityText->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), MaxQuantity)));
    }

    if (QuantitySlider)
    {
        // 슬라이더 물리 값 범위는 0.0 ~ 1.0 비율로 표준 고정하여 정밀 매핑합니다.
        QuantitySlider->SetMinValue(0.0f);
        QuantitySlider->SetMaxValue(1.0f);
    }

    UpdateQuantity(1); // 기본값은 1개로 설정
}

void UNonQuantityDialogWidget::OnSliderValueChanged(float Value)
{
    float ClampedValue = FMath::Clamp(Value, 0.0f, 1.0f);
    int32 NewQty = 1;
    if (MaxQuantity > 1)
    {
        NewQty = FMath::RoundToInt(ClampedValue * (MaxQuantity - 1)) + 1;
    }
    UpdateQuantity(NewQty, false, true); // 슬라이더 조절 중이므로 슬라이더 값 업데이트 스킵
}

void UNonQuantityDialogWidget::OnTextChanged(const FText& Text)
{
    int32 NewQty = FCString::Atoi(*Text.ToString());
    UpdateQuantity(NewQty, true, false); // 텍스트 입력 중이므로 텍스트 박스 값 업데이트 스킵
}

void UNonQuantityDialogWidget::UpdateQuantity(int32 NewQty, bool bUpdateSlider, bool bUpdateText)
{
    CurrentQuantity = FMath::Clamp(NewQty, 1, MaxQuantity);

    if (bUpdateSlider && QuantitySlider)
    {
        float Percent = 0.0f;
        if (MaxQuantity > 1)
        {
            Percent = static_cast<float>(CurrentQuantity - 1) / (MaxQuantity - 1);
        }
        QuantitySlider->SetValue(Percent);
    }

    if (bUpdateText)
    {
        if (QuantityTextBox)
        {
            QuantityTextBox->SetText(FText::AsNumber(CurrentQuantity));
        }
        else if (QuantityTextBasic)
        {
            QuantityTextBasic->SetText(FText::AsNumber(CurrentQuantity));
        }
    }
}

void UNonQuantityDialogWidget::OnConfirmClicked()
{
    OnConfirm.ExecuteIfBound(CurrentQuantity);
    RemoveFromParent();
}

void UNonQuantityDialogWidget::OnCancelClicked()
{
    RemoveFromParent();
}
