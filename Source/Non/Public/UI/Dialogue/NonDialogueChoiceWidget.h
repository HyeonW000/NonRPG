#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DialogueData.h"
#include "NonDialogueChoiceWidget.generated.h"

class UTextBlock;
class UButton;

/**
 * 대화창에서 표시될 '선택지 버튼' 위젯 기초 클래스
 */
UCLASS(Blueprintable, BlueprintType)
class NON_API UNonDialogueChoiceWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 이 선택지가 담고 있는 데이터 셋업 (숫자키 입력 시 몇 번째인지 표시)
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void SetupChoice(const FDialogueChoice& InChoiceData, class UNonUIManagerComponent* InUIManager, int32 ChoiceIndex = -1);

    // 이 버튼의 순번 (키보드 1, 2, 3 입력 매칭용)
    int32 ChoiceIndexNum = -1;

    // 키보드 입력 등으로 버튼 클릭을 강제 실행
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void SimulateClick();

protected:
    virtual void NativeConstruct() override;

    // 플레이어가 버튼을 클릭했을 때 호출될 함수
    UFUNCTION()
    void OnButtonClicked();

    // UI 매니저를 가리키는 포인터 (클릭 시 액션을 전달하기 위함)
    UPROPERTY(Transient)
    class UNonUIManagerComponent* UIManager = nullptr;

    // 이 버튼이 들고 있는 데이터 보관
    UPROPERTY(Transient)
    FDialogueChoice ChoiceData;

    // ----------------------------------------------------
    // 블루프린트에서 매칭해야 하는 변수들 (BindWidget)

    // 클릭 가능한 버튼
    UPROPERTY(meta = (BindWidget))
    UButton* ChoiceButton;

    // 버튼 위에 표시할 텍스트
    UPROPERTY(meta = (BindWidget))
    UTextBlock* ChoiceText;
};
