#include "UI/CharacterCreationWidget.h"
#include "UI/CharacterSelectWidget.h" // [New] 전방 선언 해결
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/GameplayStatics.h"
#include "System/NonSaveGame.h"

void UCharacterCreationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Class_Defender)
	{
		Btn_Class_Defender->OnClicked.AddDynamic(this, &UCharacterCreationWidget::OnClickDefender);
	}
	if (Btn_Class_Berserker)
	{
		Btn_Class_Berserker->OnClicked.AddDynamic(this, &UCharacterCreationWidget::OnClickBerserker);
	}
	if (Btn_Class_Cleric)
	{
		Btn_Class_Cleric->OnClicked.AddDynamic(this, &UCharacterCreationWidget::OnClickCleric);
	}
	if (Btn_Create)
	{
		Btn_Create->OnClicked.AddDynamic(this, &UCharacterCreationWidget::OnClickCreate);
	}
	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked.AddDynamic(this, &UCharacterCreationWidget::OnClickCancel);
	}
	if (Btn_Back)
	{
		Btn_Back->OnClicked.AddDynamic(this, &UCharacterCreationWidget::OnClickBack);
	}
	if (Btn_Next)
	{
		Btn_Next->OnClicked.AddDynamic(this, &UCharacterCreationWidget::OnClickNext);
	}

	// 초기 화면: 직업 선택 (인덱스 0)
	if (Switcher_Main)
	{
		Switcher_Main->SetActiveWidgetIndex(0);
	}
	
	if (Btn_Next) Btn_Next->SetIsEnabled(false); 
}

void UCharacterCreationWidget::OnClickDefender()
{
	SelectedClassIndex = 0; // Defender
	if (Btn_Next) Btn_Next->SetIsEnabled(true);
}

void UCharacterCreationWidget::OnClickBerserker()
{
	SelectedClassIndex = 1; // Berserker
	if (Btn_Next) Btn_Next->SetIsEnabled(true);
}

void UCharacterCreationWidget::OnClickCleric()
{
	SelectedClassIndex = 2; // Cleric
	if (Btn_Next) Btn_Next->SetIsEnabled(true);
}

void UCharacterCreationWidget::OnClickNext()
{
	// 다음 화면(세부 설정)으로 이동
	if (Switcher_Main)
	{
		Switcher_Main->SetActiveWidgetIndex(1);
	}
}

void UCharacterCreationWidget::OnClickBack()
{
	// 이전 화면(직업 선택)으로 돌아가기
	if (Switcher_Main)
	{
		Switcher_Main->SetActiveWidgetIndex(0);
	}
}

void UCharacterCreationWidget::OnClickCreate()
{
	FString NewName = TEXT("Player");
	if (Input_Name)
	{
		NewName = Input_Name->GetText().ToString();
	}

	if (NewName.IsEmpty())
	{
		// TODO: 경고 메시지
		return;
	}

	// 1. SaveGame 객체 생성
	UNonSaveGame* NewData = Cast<UNonSaveGame>(UGameplayStatics::CreateSaveGameObject(UNonSaveGame::StaticClass()));
	if (NewData)
	{
		// 2. 초기 데이터 세팅
		NewData->PlayerName = NewName;
		NewData->JobClass = (EJobClass)SelectedClassIndex; // Enum 캐스팅
		NewData->Level = 1;
		NewData->CurrentHP = 100.f; // 직업별로 다르게 하려면 데이터 테이블 참조 필요
		NewData->CurrentMP = 100.f;
		
	// 3. 저장 (전달받은 SlotIndex 사용)
		UGameplayStatics::SaveGameToSlot(NewData, FString::Printf(TEXT("Slot%d"), TargetSlotIndex), 0);
	}

	// 4. 완료 후 처리: 다시 선택 화면으로 돌아가거나, 바로 게임 시작?
	// 보통은 선택 화면으로 돌아가서 갱신된 모습을 보여줌
	RemoveFromParent();
	
	if (ParentSelectWidget)
	{
		ParentSelectWidget->SetVisibility(ESlateVisibility::Visible);
		ParentSelectWidget->UpdateSlotDisplay();
		// 편의상 바로 선택 상태로 만들어주면 좋음
	}
}

void UCharacterCreationWidget::OnClickCancel()
{
	RemoveFromParent();
	if (ParentSelectWidget)
	{
		ParentSelectWidget->SetVisibility(ESlateVisibility::Visible);
	}
}
