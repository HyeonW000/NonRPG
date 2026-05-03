#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NonDialogueWidget.generated.h"

class UTextBlock;

/**
 * 대화창 UI 위젯 기초 클래스
 */
UCLASS(Blueprintable, BlueprintType)
class NON_API UNonDialogueWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // NPC 이름과 대사 텍스트를 업데이트하는 함수
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void SetDialogueData(const FText& NPCName, const FText& DialogueLine);

    // 대화창이 켜질 때 애니메이션을 재생하는 함수
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void PlayFadeIn();

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void ClearChoices();

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void AddChoiceWidget(class UNonDialogueChoiceWidget* ChoiceWidget);

    // 동적으로 생성된 선택지 버튼들을 담을 박스 (블루프린트의 VerticalBox 이름과 동일하게)
    UPROPERTY(meta = (BindWidgetOptional))
    class UVerticalBox* ChoicesContainer;

protected:
    // 블루프린트 위젯에서 똑같은 이름(NPCNameText)으로 만든 TextBlock과 자동으로 연동됩니다.
    UPROPERTY(meta = (BindWidget))
    UTextBlock* NPCNameText;

    // 블루프린트 위젯에서 똑같은 이름(DialogueText)으로 만든 TextBlock과 자동으로 연동됩니다.
    UPROPERTY(meta = (BindWidget))
    UTextBlock* DialogueText;

    // 위젯 애니메이션 (블루프린트에서 FadeInAnim 이라는 이름으로 만들면 연동됨)
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    class UWidgetAnimation* FadeInAnim;

    // 키보드 입력을 받기 위해 포커스를 지원합니다.
    virtual bool NativeSupportsKeyboardFocus() const override { return true; }

    // 숫자 키 입력을 가로채서 선택지 선택
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    virtual void NativeConstruct() override;
};
