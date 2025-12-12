#include "UI/CharacterSelectWidget.h"
#include "UI/CharacterCreationWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "System/NonSaveGame.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h" // [New]
#include "System/NonGameInstance.h" // [New]

void UCharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Slot_0) Btn_Slot_0->OnClicked.AddDynamic(this, &UCharacterSelectWidget::OnClickSlot0);
	if (Btn_Slot_1) Btn_Slot_1->OnClicked.AddDynamic(this, &UCharacterSelectWidget::OnClickSlot1);
	if (Btn_Slot_2) Btn_Slot_2->OnClicked.AddDynamic(this, &UCharacterSelectWidget::OnClickSlot2);
	
	if (Btn_StartGame) 
	{
		Btn_StartGame->OnClicked.AddDynamic(this, &UCharacterSelectWidget::OnClickStartGame);
		Btn_StartGame->SetIsEnabled(false); // 처음엔 비활성화
	}
	if (Btn_Delete)
	{
		Btn_Delete->OnClicked.AddDynamic(this, &UCharacterSelectWidget::OnClickDelete);
		Btn_Delete->SetIsEnabled(false);
	}

	UpdateSlotDisplay();
}

void UCharacterSelectWidget::UpdateSlotDisplay()
{
	// 슬롯별 데이터 확인 로직
	auto CheckSlot = [&](int32 Index, UTextBlock* TextComp, UImage* ImageComp) {
		FString SlotName = FString::Printf(TEXT("Slot%d"), Index);
		if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
		{
			// 로드해서 이름 표시
			if (UNonSaveGame* Data = Cast<UNonSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
			{
				TextComp->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d %s"), Data->Level, *Data->PlayerName)));
				
				// 아이콘 설정 [New]
				if (ImageComp)
				{
					if (UTexture2D** FoundIcon = ClassIcons.Find(Data->JobClass))
					{
						ImageComp->SetBrushFromTexture(*FoundIcon);
						ImageComp->SetVisibility(ESlateVisibility::Visible);
					}
					else
					{
						ImageComp->SetVisibility(ESlateVisibility::Hidden); 
					}
				}
			}
		}
		else
		{
			TextComp->SetText(FText::FromString(TEXT("캐릭터 생성")));
			// 데이터 없으면 아이콘 숨김
			if (ImageComp) ImageComp->SetVisibility(ESlateVisibility::Hidden);
		}
	};

	if (Text_Slot_0) CheckSlot(0, Text_Slot_0, Image_Slot_0);
	if (Text_Slot_1) CheckSlot(1, Text_Slot_1, Image_Slot_1);
	if (Text_Slot_2) CheckSlot(2, Text_Slot_2, Image_Slot_2);
	
	// 3D 캐릭터 갱신
	RefreshLobbyCharacters();
}

void UCharacterSelectWidget::HandleSlotClick(int32 SlotIndex)
{
	FString SlotName = FString::Printf(TEXT("Slot%d"), SlotIndex);

	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		// 1. 이미 캐릭터가 있음 -> 선택(Highlight) -> 게임 시작 버튼 활성화
		SelectedSlotIndex = SlotIndex;
		if (Btn_StartGame) Btn_StartGame->SetIsEnabled(true);
		if (Btn_Delete) Btn_Delete->SetIsEnabled(true); // 삭제 가능
		
		// TODO: 선택된 슬롯 시각적 강조 (테두리 등)
	}
	else
	{
		// 2. 비어 있음 -> 생성 창으로 이동
		if (CreationWidgetClass)
		{
			// 현재 위젯 숨기기
			SetVisibility(ESlateVisibility::Hidden);

			// 생성 위젯 띄우기 (CreateWidget 섀도잉 방지를 위해 풀네임 호출 추천)
			UCharacterCreationWidget* NewWidget = CreateWidget<UCharacterCreationWidget>(GetWorld(), CreationWidgetClass);
			if (NewWidget)
			{
				NewWidget->TargetSlotIndex = SlotIndex; // 슬롯 번호 전달
				NewWidget->ParentSelectWidget = this;   // 돌아올 곳 전달
				NewWidget->AddToViewport();
			}
		}
	}
}

void UCharacterSelectWidget::OnClickSlot0() { HandleSlotClick(0); }
void UCharacterSelectWidget::OnClickSlot1() { HandleSlotClick(1); }
void UCharacterSelectWidget::OnClickSlot2() { HandleSlotClick(2); }

void UCharacterSelectWidget::OnClickStartGame()
{
	if (SelectedSlotIndex < 0) return;

	// **중요**: 선택된 슬롯 정보를 GameInstance 등에 저장해서 메인 게임이 알게 해야 함
	// 여기서는 단순히 레벨만 엽니다. 실제로는 "CurrentSlotName"을 어딘가 전역 변수에 세팅해야 함.
	
	if (SelectedSlotIndex < 0) return;

	// GameInstance에 선택된 슬롯 정보 저장
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UNonGameInstance* NonGI = Cast<UNonGameInstance>(GI))
		{
			NonGI->CurrentSlotName = FString::Printf(TEXT("Slot%d"), SelectedSlotIndex);
		}
	}
	
	// [Revert] 강제 GameMode 옵션 제거
	// 이유: 월드 세팅에서 BP_NonGameMode를 사용하도록 권장했으므로, 
	// 코드에서 C++ GameMode를 강제하면 BP 설정(Controller 등)이 무시됨.
	UGameplayStatics::OpenLevel(this, FName("LV_StartMap"));
}

void UCharacterSelectWidget::OnClickDelete()
{
	if (SelectedSlotIndex < 0) return;

	FString SlotName = FString::Printf(TEXT("Slot%d"), SelectedSlotIndex);
	
	// 세이브 파일 삭제
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}

	// 선택 초기화 및 UI 갱신
	SelectedSlotIndex = -1;
	if (Btn_StartGame) Btn_StartGame->SetIsEnabled(false);
	if (Btn_Delete) Btn_Delete->SetIsEnabled(false);

	UpdateSlotDisplay(); // 텍스트 갱신 (-> 캐릭터 생성)
	RefreshLobbyCharacters(); // 3D 캐릭터 숨기기
}

void UCharacterSelectWidget::RefreshLobbyCharacters()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 슬롯 0, 1, 2에 대해 반복
	for (int32 i = 0; i < 3; i++)
	{
		FString SlotName = FString::Printf(TEXT("Slot%d"), i);
		bool bHasData = UGameplayStatics::DoesSaveGameExist(SlotName, 0);

		// 태그: LobbyChar0, LobbyChar1, LobbyChar2
		FName TagName = *FString::Printf(TEXT("LobbyChar%d"), i);
		
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(World, TagName, FoundActors);

		for (AActor* Actor : FoundActors)
		{
			// 데이터 있으면 보이고, 없으면 숨김
			Actor->SetActorHiddenInGame(!bHasData);
		}
	}
}
