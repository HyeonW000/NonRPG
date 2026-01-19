#include "UI/CharacterCreationWidget.h"
#include "UI/CharacterSelectWidget.h" // [New] 전방 선언 해결
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/Border.h" // [Fix] Include 추가
#include "Components/WidgetSwitcher.h"
#include "Kismet/GameplayStatics.h"
#include "System/NonSaveGame.h"
#include "Character/NonCharacterBase.h" // [New]
#include "GameFramework/Actor.h"
#include "Core/LobbyPlayerController.h" // [New] for OnCharacterCreationFinished
#include "Core/NonPlayerController.h" // [New]
#include "System/NonGameInstance.h" // [New]

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

	// [New] 미리보기 캐릭터 찾기 ("PreviewChar" 태그)
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("PreviewChar"), FoundActors);
	if (FoundActors.Num() > 0)
	{
		PreviewCharacter = Cast<ANonCharacterBase>(FoundActors[0]);
		PreviewCharacter->SetActorHiddenInGame(false); // [Fix] 강제 보이기 (혹시 숨겨져 있을 경우 대비)

	}
	else
	{

	}

	// [New] 초기 하이라이트 및 미리보기 (기본값 설정 안함, 선택 시 갱신)
	if (Border_Class_Defender) Border_Class_Defender->SetVisibility(ESlateVisibility::Hidden);
	if (Border_Class_Berserker) Border_Class_Berserker->SetVisibility(ESlateVisibility::Hidden);
	if (Border_Class_Cleric) Border_Class_Cleric->SetVisibility(ESlateVisibility::Hidden); 

	// [New] 카메라 전환
	if (APlayerController* PC = GetOwningPlayer())
	{
		// 1. 기존 카메라 저장
		OriginalViewTarget = PC->GetViewTarget();

		// 2. 새 카메라 찾기 ("PreviewCamera" 태그)
		TArray<AActor*> FoundCameras;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("PreviewCamera"), FoundCameras);
		if (FoundCameras.Num() > 0)
		{
			PreviewCameraActor = FoundCameras[0];
			// 0.5초 동안 부드럽게 이동
			PC->SetViewTargetWithBlend(PreviewCameraActor, 0.5f);


			if (PreviewCharacter)
			{
				// [Fix] 처음에는 숨겨둠 -> 직업 버튼 누르면 등장
				PreviewCharacter->SetActorHiddenInGame(true);

			}
		}
		else
		{

		}
	} 
}

void UCharacterCreationWidget::OnClickDefender()
{
	SelectedClassIndex = (int32)EJobClass::Defender; // 1
	if (Btn_Next) Btn_Next->SetIsEnabled(true);

	// Highlight Update
	if (Border_Class_Defender) Border_Class_Defender->SetVisibility(ESlateVisibility::Visible);
	if (Border_Class_Berserker) Border_Class_Berserker->SetVisibility(ESlateVisibility::Hidden);
	if (Border_Class_Cleric) Border_Class_Cleric->SetVisibility(ESlateVisibility::Hidden);

	UpdatePreviewModel(EJobClass::Defender); 
}

void UCharacterCreationWidget::OnClickBerserker()
{
	SelectedClassIndex = (int32)EJobClass::Berserker; // 2
	if (Btn_Next) Btn_Next->SetIsEnabled(true);

	// Highlight Update
	if (Border_Class_Defender) Border_Class_Defender->SetVisibility(ESlateVisibility::Hidden);
	if (Border_Class_Berserker) Border_Class_Berserker->SetVisibility(ESlateVisibility::Visible);
	if (Border_Class_Cleric) Border_Class_Cleric->SetVisibility(ESlateVisibility::Hidden);

	UpdatePreviewModel(EJobClass::Berserker);
}

void UCharacterCreationWidget::OnClickCleric()
{
	SelectedClassIndex = (int32)EJobClass::Cleric; // 3
	if (Btn_Next) Btn_Next->SetIsEnabled(true);

	// Highlight Update
	if (Border_Class_Defender) Border_Class_Defender->SetVisibility(ESlateVisibility::Hidden);
	if (Border_Class_Berserker) Border_Class_Berserker->SetVisibility(ESlateVisibility::Hidden);
	if (Border_Class_Cleric) Border_Class_Cleric->SetVisibility(ESlateVisibility::Visible);

	UpdatePreviewModel(EJobClass::Cleric);
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
    FString SaveSlotName = FString::Printf(TEXT("Slot%d"), TargetSlotIndex);
    if (UNonGameInstance* GI = Cast<UNonGameInstance>(GetGameInstance()))
    {
        SaveSlotName = GI->GetSaveSlotName(TargetSlotIndex);
    }

		UGameplayStatics::SaveGameToSlot(NewData, SaveSlotName, 0);
	}

	// 4. 완료 후 처리: 다시 선택 화면으로 돌아가거나, 바로 게임 시작?
	// -> 로비 컨트롤러에게 "나 끝났어! 화면이랑 카메라 원복해줘!" 요청
	if (ALobbyPlayerController* PC = Cast<ALobbyPlayerController>(GetOwningPlayer()))
	{
		PC->OnCharacterCreationFinished();
	}
    // [New] MMO 컨트롤러 지원
    else if (ANonPlayerController* NonPC = Cast<ANonPlayerController>(GetOwningPlayer()))
    {
        NonPC->OnCharacterCreationFinished();
    }

	// [Legacy] 수동 복구 로직 제거
	/*
	RemoveFromParent();
	if (ParentSelectWidget)
	{
		ParentSelectWidget->SetVisibility(ESlateVisibility::Visible);
		ParentSelectWidget->UpdateSlotDisplay();
	}
	*/
}

void UCharacterCreationWidget::UpdatePreviewModel(EJobClass Job)
{
	if (PreviewCharacter)
	{
		// [Move] 버튼을 눌렀으니 이제 보여줌!
		PreviewCharacter->SetActorHiddenInGame(false);

		// 1레벨 기준으로 해당 직업의 외형으로 초기화 (스탯은 상관없음)
		PreviewCharacter->InitCharacterData(Job, 1);
		
		// (선택) 무기까지 들어주려면 Armed 상태로 설정
		// PreviewCharacter->SetArmed(true); 
		// PreviewCharacter->RefreshWeaponStance(); 
	}
}

void UCharacterCreationWidget::OnClickCancel()
{
	// [New] 컨트롤러를 통해 취소 처리 (카메라/UI 복구)
	if (ALobbyPlayerController* PC = Cast<ALobbyPlayerController>(GetOwningPlayer()))
	{
		PC->OnCharacterCreationFinished();
	}
    // [New] MMO 컨트롤러 지원
    else if (ANonPlayerController* NonPC = Cast<ANonPlayerController>(GetOwningPlayer()))
    {
        NonPC->OnCharacterCreationFinished();
    }
	
	/* [Legacy] 수동 복구 로직 제거
	if (OriginalViewTarget)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->SetViewTargetWithBlend(OriginalViewTarget, 0.5f);
		}
	}

	RemoveFromParent();
	if (ParentSelectWidget)
	{
		ParentSelectWidget->SetVisibility(ESlateVisibility::Visible);
	}
	*/
}
