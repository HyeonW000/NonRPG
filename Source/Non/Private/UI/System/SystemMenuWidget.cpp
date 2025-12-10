#include "UI/System/SystemMenuWidget.h"
#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Core/NonUIManagerComponent.h"

void USystemMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (CloseButton)
    {
        CloseButton->OnClicked.RemoveAll(this);
        CloseButton->OnClicked.AddDynamic(this, &USystemMenuWidget::OnCloseClicked);
    }

    if (QuitButton)
    {
        QuitButton->OnClicked.RemoveAll(this);
        QuitButton->OnClicked.AddDynamic(this, &USystemMenuWidget::OnQuitClicked);
    }

    // 포커스 가능 설정
    SetIsFocusable(true);
}

void USystemMenuWidget::Init(UNonUIManagerComponent* InUIManager)
{
    UIManager = InUIManager;
}

void USystemMenuWidget::OnCloseClicked()
{
    if (UIManager.IsValid())
    {
        UIManager->CloseWindow(EGameWindowType::SystemMenu);
    }
    else
    {
        SetVisibility(ESlateVisibility::Collapsed);
    }
}

#include "System/SaveGameSubsystem.h"
#include "Kismet/GameplayStatics.h"

// ... existing code ...

void USystemMenuWidget::OnQuitClicked()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (USaveGameSubsystem* SaveSys = GI->GetSubsystem<USaveGameSubsystem>())
        {
            SaveSys->SaveGame();
        }
    }

    if (APlayerController* PC = GetOwningPlayer())
    {
        UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, true);
    }
}
