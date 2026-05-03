#include "UI/Dialogue/NonDialogueWidget.h"
#include "Components/TextBlock.h"

#include "Components/VerticalBox.h"
#include "UI/Dialogue/NonDialogueChoiceWidget.h"

void UNonDialogueWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UNonDialogueWidget::PlayFadeIn()
{
    if (FadeInAnim)
    {
        PlayAnimation(FadeInAnim);
    }
}

void UNonDialogueWidget::ClearChoices()
{
    if (ChoicesContainer)
    {
        ChoicesContainer->ClearChildren();
    }
}

void UNonDialogueWidget::AddChoiceWidget(UNonDialogueChoiceWidget* ChoiceWidget)
{
    if (ChoicesContainer && ChoiceWidget)
    {
        ChoicesContainer->AddChild(ChoiceWidget);
    }
}

void UNonDialogueWidget::SetDialogueData(const FText& InNPCName, const FText& InDialogueLine)
{
    // 이름 텍스트가 정상적으로 바인딩되어 있으면 값 변경
    if (NPCNameText)
    {
        NPCNameText->SetText(InNPCName);
    }

    // 대사 텍스트가 정상적으로 바인딩되어 있으면 값 변경
    if (DialogueText)
    {
        DialogueText->SetText(InDialogueLine);
    }
}

FReply UNonDialogueWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (!ChoicesContainer || ChoicesContainer->GetChildrenCount() == 0)
    {
        return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
    }

    int32 SelectedIndex = -1;
    FKey PressedKey = InKeyEvent.GetKey();

    if (PressedKey == EKeys::One || PressedKey == EKeys::NumPadOne) SelectedIndex = 0;
    else if (PressedKey == EKeys::Two || PressedKey == EKeys::NumPadTwo) SelectedIndex = 1;
    else if (PressedKey == EKeys::Three || PressedKey == EKeys::NumPadThree) SelectedIndex = 2;
    else if (PressedKey == EKeys::Four || PressedKey == EKeys::NumPadFour) SelectedIndex = 3;
    else if (PressedKey == EKeys::Five || PressedKey == EKeys::NumPadFive) SelectedIndex = 4;
    else if (PressedKey == EKeys::Six || PressedKey == EKeys::NumPadSix) SelectedIndex = 5;
    else if (PressedKey == EKeys::Seven || PressedKey == EKeys::NumPadSeven) SelectedIndex = 6;
    else if (PressedKey == EKeys::Eight || PressedKey == EKeys::NumPadEight) SelectedIndex = 7;
    else if (PressedKey == EKeys::Nine || PressedKey == EKeys::NumPadNine) SelectedIndex = 8;

    if (SelectedIndex >= 0 && SelectedIndex < ChoicesContainer->GetChildrenCount())
    {
        if (UNonDialogueChoiceWidget* ChoiceWidget = Cast<UNonDialogueChoiceWidget>(ChoicesContainer->GetChildAt(SelectedIndex)))
        {
            ChoiceWidget->SimulateClick();
            return FReply::Handled();
        }
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
