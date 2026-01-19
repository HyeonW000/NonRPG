#include "UI/System/SystemMenuWidget.h"
#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Core/NonUIManagerComponent.h"
#include "System/NonGameInstance.h" // [New]

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

    if (CharacterSelectButton)
    {
        CharacterSelectButton->OnClicked.RemoveAll(this);
        CharacterSelectButton->OnClicked.AddDynamic(this, &USystemMenuWidget::OnCharacterSelectClicked);
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

void USystemMenuWidget::OnCharacterSelectClicked()
{
    // 1. 자동 저장
    if (UGameInstance* GI = GetGameInstance())
    {
        if (USaveGameSubsystem* SaveSys = GI->GetSubsystem<USaveGameSubsystem>())
        {
            SaveSys->SaveGame();
        }
    }

    // 2. 로비 레벨로 이동 (OpenLevel)
    if (UWorld* World = GetWorld())
    {
        // [New] 타이틀 스킵 설정
        if (UNonGameInstance* GI = Cast<UNonGameInstance>(GetGameInstance()))
        {
            GI->bSkipTitle = true;
        }

        // "LV_Lobby"는 하드코딩된 로비 맵 이름 (변경 시 수정 필요)
        UGameplayStatics::OpenLevel(World, FName("LV_Lobby"));
    }
}
