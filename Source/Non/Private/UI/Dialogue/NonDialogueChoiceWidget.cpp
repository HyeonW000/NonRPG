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

    if (ChoiceText)
    {
        if (ChoiceIndex > 0)
        {
            // [1] 상점 열기 형태로 표시
            FString FormattedString = FString::Printf(TEXT("[%d] %s"), ChoiceIndex, *ChoiceData.ChoiceText.ToString());
            ChoiceText->SetText(FText::FromString(FormattedString));
        }
        else
        {
            ChoiceText->SetText(ChoiceData.ChoiceText);
        }
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
