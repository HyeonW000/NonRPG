#include "UI/Dialogue/NonDialogueChoiceWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/NonUIManagerComponent.h"

void UNonDialogueChoiceWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (ChoiceButton)
    {
        // 이미 바인딩되어 있다면 안전하게 제거 후 추가
        ChoiceButton->OnClicked.RemoveDynamic(this, &UNonDialogueChoiceWidget::OnButtonClicked);
        ChoiceButton->OnClicked.AddDynamic(this, &UNonDialogueChoiceWidget::OnButtonClicked);
    }
}

void UNonDialogueChoiceWidget::SetupChoice(const FDialogueChoice& InChoiceData, UNonUIManagerComponent* InUIManager, int32 ChoiceIndex)
{
    ChoiceData = InChoiceData;
    UIManager = InUIManager;
    ChoiceIndexNum = ChoiceIndex;

    // 선택지 번호가 있을 때
    if (ChoiceIndex > 0)
    {
        // 1) 별도의 숫자 전용 텍스트(NumberText)가 있다면 거기에 표시
        if (NumberText)
        {
            NumberText->SetText(FText::AsNumber(ChoiceIndex));
            
            // 이 경우 메인 텍스트는 번호 없이 원본 그대로 표시
            if (ChoiceText)
            {
                ChoiceText->SetText(ChoiceData.ChoiceText);
            }
        }
        // 2) 전용 숫자 텍스트가 없다면, 기존 방식대로 메인 텍스트 앞에 [1] 처럼 붙임
        else if (ChoiceText)
        {
            FString FormattedString = FString::Printf(TEXT("[%d] %s"), ChoiceIndex, *ChoiceData.ChoiceText.ToString());
            ChoiceText->SetText(FText::FromString(FormattedString));
        }
    }
    else if (ChoiceText)
    {
        ChoiceText->SetText(ChoiceData.ChoiceText);
    }
}

void UNonDialogueChoiceWidget::SimulateClick()
{
    OnButtonClicked();
}

void UNonDialogueChoiceWidget::OnButtonClicked()
{
    if (UIManager)
    {
        // UIManager에게 유저가 이 선택지를 골랐다고 알려줍니다.
        UIManager->OnDialogueChoiceSelected(ChoiceData);
    }
}
