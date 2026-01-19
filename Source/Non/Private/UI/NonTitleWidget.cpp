#include "UI/NonTitleWidget.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Core/LobbyPlayerController.h"
#include "Core/NonPlayerController.h" // [New]

void UNonTitleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 초기 상태 설정
	if (MenuContainer)
	{
		MenuContainer->SetVisibility(ESlateVisibility::Hidden);
	}
	if (Btn_AnyKey)
	{
		Btn_AnyKey->SetVisibility(ESlateVisibility::Visible);
		Btn_AnyKey->OnClicked.AddDynamic(this, &ThisClass::OnAnyKeyClicked);
	}

	// 델리게이트 연결
	if (Btn_Play)   Btn_Play->OnClicked.AddDynamic(this, &ThisClass::OnPlayClicked);
	if (Btn_Option) Btn_Option->OnClicked.AddDynamic(this, &ThisClass::OnOptionClicked);
	if (Btn_Exit)   Btn_Exit->OnClicked.AddDynamic(this, &ThisClass::OnExitClicked);
}

void UNonTitleWidget::OnAnyKeyClicked()
{
	// 1. AnyKey 버튼 숨김 (더 이상 클릭 안 되게)
	if (Btn_AnyKey)
	{
		Btn_AnyKey->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 2. 메뉴 표시
	if (MenuContainer)
	{
		MenuContainer->SetVisibility(ESlateVisibility::Visible);
	}

	// 3. 애니메이션 재생
	if (Anim_ShowMenu)
	{
		PlayAnimation(Anim_ShowMenu);
	}
}

void UNonTitleWidget::OnPlayClicked()
{
	// 로비 컨트롤러를 찾아서 캐릭터 선택창으로 전환 요청
	if (ALobbyPlayerController* PC = Cast<ALobbyPlayerController>(GetOwningPlayer()))
	{
		PC->StartCharacterSelect();
	}
    // [New] MMO 스타일 접속 컨트롤러 지원
    else if (ANonPlayerController* NonPC = Cast<ANonPlayerController>(GetOwningPlayer()))
    {
        NonPC->GoToCharacterSelect();
    }
}

void UNonTitleWidget::OnOptionClicked()
{
	// 옵션 창 구현 예정 (일단 로그만)
	// UE_LOG(LogTemp, Log, TEXT("Option Clicked"));
}

void UNonTitleWidget::OnExitClicked()
{
	// 게임 종료
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, true);
}
