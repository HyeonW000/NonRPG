#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SystemMenuWidget.generated.h"

class UButton;
class UNonUIManagerComponent;

/**
 * ESC 메뉴 (시스템 메뉴)
 * - 닫기 (Close/Back)
 * - 종료 (Quit)
 */
UCLASS()
class NON_API USystemMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

protected:
    // UI 매니저 캐싱
    TWeakObjectPtr<UNonUIManagerComponent> UIManager;

    // --- Bind Widgets ---
    UPROPERTY(meta = (BindWidgetOptional))
    UButton* CloseButton;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* QuitButton;

    // --- Functions ---
    UFUNCTION()
    void OnCloseClicked();

    UFUNCTION()
    void OnQuitClicked();

public:
    // UI 매니저 주입
    void Init(UNonUIManagerComponent* InUIManager);
};
