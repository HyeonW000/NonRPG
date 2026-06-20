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
    if (Slider_Quantity || Text_MaxQuantity || EditableText_Quantity_Box || EditableText_Quantity_Basic)
    {
        RefreshDialogUI();
    }
}

void UNonQuantityDialogWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 블루프린트 위젯 내의 이름 매핑 (UI 레이아웃과의 컴파일 디펜던시 완전 차단)
    Slider_Quantity = Cast<USlider>(GetWidgetFromName(TEXT("Slider_Quantity")));
    
    // UEditableTextBox 와 UEditableText 둘 다 유연하게 대응
    UWidget* FoundTextWidget = GetWidgetFromName(TEXT("EditableText_Quantity"));
    EditableText_Quantity_Box = Cast<UEditableTextBox>(FoundTextWidget);
    EditableText_Quantity_Basic = Cast<UEditableText>(FoundTextWidget);

    Button_Confirm = Cast<UButton>(GetWidgetFromName(TEXT("Button_Confirm")));
    Button_Cancel = Cast<UButton>(GetWidgetFromName(TEXT("Button_Cancel")));
    Text_MaxQuantity = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_MaxQuantity")));

    // 런타임 바인딩 상태 디버그 출력
    UE_LOG(LogTemp, Warning, TEXT("[QuantityDialog] UI 컴포넌트 변수 바인딩 상태 리포트:"));
    UE_LOG(LogTemp, Warning, TEXT(" -> Slider_Quantity: %s"), Slider_Quantity ? TEXT("바인딩 성공") : TEXT("바인딩 실패 (이름 대소문자 또는 IsVariable 확인 필요)"));
    
    bool bTextBindingSuccess = (EditableText_Quantity_Box != nullptr) || (EditableText_Quantity_Basic != nullptr);
    FString TextWidgetType = EditableText_Quantity_Box ? TEXT("EditableTextBox 타입") : (EditableText_Quantity_Basic ? TEXT("EditableText 타입") : TEXT("없음"));
    UE_LOG(LogTemp, Warning, TEXT(" -> EditableText_Quantity: %s (%s)"), bTextBindingSuccess ? TEXT("바인딩 성공") : TEXT("바인딩 실패 (이름 대소문자 또는 IsVariable 확인 필요)"), *TextWidgetType);
    
    UE_LOG(LogTemp, Warning, TEXT(" -> Button_Confirm: %s"), Button_Confirm ? TEXT("바인딩 성공") : TEXT("바인딩 실패 (이름 대소문자 또는 IsVariable 확인 필요)"));
    UE_LOG(LogTemp, Warning, TEXT(" -> Button_Cancel: %s"), Button_Cancel ? TEXT("바인딩 성공") : TEXT("바인딩 실패 (이름 대소문자 또는 IsVariable 확인 필요)"));
    UE_LOG(LogTemp, Warning, TEXT(" -> Text_MaxQuantity: %s"), Text_MaxQuantity ? TEXT("바인딩 성공") : TEXT("바인딩 실패 (이름 대소문자 또는 IsVariable 확인 필요 - 선택사항)"));

    if (Slider_Quantity)
    {
        Slider_Quantity->OnValueChanged.AddDynamic(this, &UNonQuantityDialogWidget::OnSliderValueChanged);
    }

    if (EditableText_Quantity_Box)
    {
        EditableText_Quantity_Box->OnTextChanged.AddDynamic(this, &UNonQuantityDialogWidget::OnTextChanged);
    }
    else if (EditableText_Quantity_Basic)
    {
        EditableText_Quantity_Basic->OnTextChanged.AddDynamic(this, &UNonQuantityDialogWidget::OnTextChanged);
    }

    if (Button_Confirm)
    {
        Button_Confirm->OnClicked.AddDynamic(this, &UNonQuantityDialogWidget::OnConfirmClicked);
    }

    if (Button_Cancel)
    {
        Button_Cancel->OnClicked.AddDynamic(this, &UNonQuantityDialogWidget::OnCancelClicked);
    }

    // 만약 SetupDialog가 먼저 호출되어 변수가 준비되어 있다면 UI 세팅을 갱신합니다.
    if (bIsInitialized)
    {
        RefreshDialogUI();
    }
}

void UNonQuantityDialogWidget::RefreshDialogUI()
{
    if (Text_MaxQuantity)
    {
        Text_MaxQuantity->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), MaxQuantity)));
    }

    if (Slider_Quantity)
    {
        // 슬라이더 물리 값 범위는 0.0 ~ 1.0 비율로 표준 고정하여 정밀 매핑합니다.
        Slider_Quantity->SetMinValue(0.0f);
        Slider_Quantity->SetMaxValue(1.0f);
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

    if (bUpdateSlider && Slider_Quantity)
    {
        float Percent = 0.0f;
        if (MaxQuantity > 1)
        {
            Percent = static_cast<float>(CurrentQuantity - 1) / (MaxQuantity - 1);
        }
        Slider_Quantity->SetValue(Percent);
    }

    if (bUpdateText)
    {
        if (EditableText_Quantity_Box)
        {
            EditableText_Quantity_Box->SetText(FText::AsNumber(CurrentQuantity));
        }
        else if (EditableText_Quantity_Basic)
        {
            EditableText_Quantity_Basic->SetText(FText::AsNumber(CurrentQuantity));
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
