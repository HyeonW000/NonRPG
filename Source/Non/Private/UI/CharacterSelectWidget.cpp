#include "UI/CharacterSelectWidget.h"
#include "UI/CharacterCreationWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h" // [New]
#include "Kismet/GameplayStatics.h"
#include "System/NonSaveGame.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h" // [New]
#include "System/NonGameInstance.h" // [New]
#include "Engine/Texture2D.h" // [New]
#include "System/NonGameInstance.h" // [New]
#include "Core/LobbyPlayerController.h" // [New] for StartCharacterCreation
#include "Core/NonPlayerController.h" // [New] for ServerSpawnCharacter
#include "Character/NonCharacterBase.h" // [New] for Lobby Equipment
#include "Equipment/EquipmentComponent.h" // [New] for RestoreEquippedItemsFromSave
#include "Inventory/InventoryComponent.h" // [New] for ItemDataTable

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

	// 초기 보더 상태: 모두 숨김
	if (Border_Slot_0) Border_Slot_0->SetVisibility(ESlateVisibility::Hidden);
	if (Border_Slot_1) Border_Slot_1->SetVisibility(ESlateVisibility::Hidden);
	if (Border_Slot_2) Border_Slot_2->SetVisibility(ESlateVisibility::Hidden);

	UpdateSlotDisplay();
}

void UCharacterSelectWidget::UpdateSlotDisplay()
{
	// 슬롯별 데이터 확인 로직
	auto CheckSlot = [&](int32 Index, UTextBlock* TextComp, UImage* ImageComp) {
		FString SlotName = FString::Printf(TEXT("Slot%d"), Index);
        if (UNonGameInstance* GI = Cast<UNonGameInstance>(GetGameInstance()))
        {
            SlotName = GI->GetSaveSlotName(Index);
        }

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
    if (UNonGameInstance* GI = Cast<UNonGameInstance>(GetGameInstance()))
    {
        SlotName = GI->GetSaveSlotName(SlotIndex);
    }

	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		// 1. 이미 캐릭터가 있음 -> 선택(Highlight) -> 게임 시작 버튼 활성화
		SelectedSlotIndex = SlotIndex;
		if (Btn_StartGame) Btn_StartGame->SetIsEnabled(true);
		if (Btn_Delete) Btn_Delete->SetIsEnabled(true); // 삭제 가능
		
		// 선택된 슬롯 시각적 강조 (테두리 표시)
		if (Border_Slot_0) Border_Slot_0->SetVisibility(SlotIndex == 0 ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		if (Border_Slot_1) Border_Slot_1->SetVisibility(SlotIndex == 1 ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		if (Border_Slot_2) Border_Slot_2->SetVisibility(SlotIndex == 2 ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
    else
    {
        // 2. 비어 있음 -> 생성 창으로 이동
        // 기존 코드: 직접 위젯 생성 -> [변경] 컨트롤러에게 위임 (카메라 전환 포함)
        
        if (ALobbyPlayerController* PC = Cast<ALobbyPlayerController>(GetOwningPlayer()))
        {
            // [Important] 컨트롤러에게 선택한 슬롯 번호도 같이 전달
            PC->StartCharacterCreation(SlotIndex);
        }
        // [New] MMO 스타일 컨트롤러 지원
        else if (ANonPlayerController* NonPC = Cast<ANonPlayerController>(GetOwningPlayer()))
        {
             if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("[UI] Requesting Character Creation..."));
            NonPC->StartCharacterCreation(SlotIndex);
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
            // GI->CurrentSlotName = FString::Printf(TEXT("Slot%d"), SelectedSlotIndex);
			NonGI->CurrentSlotName = NonGI->GetSaveSlotName(SelectedSlotIndex);
            NonGI->CurrentSlotIndex = SelectedSlotIndex; // [New] 인덱스 직접 저장
		}
	}
	
	// [Revert] 강제 GameMode 옵션 제거
	// 이유: 월드 세팅에서 BP_NonGameMode를 사용하도록 권장했으므로, 
	// 코드에서 C++ GameMode를 강제하면 BP 설정(Controller 등)이 무시됨.
    
    // [Changed] 오프라인 로비 -> 서버 접속 방식 (OpenLevel with Options)
    // 1. 옵션 문자열 생성 (?Slot=0)
    FString Options = FString::Printf(TEXT("?Slot=%d"), SelectedSlotIndex);
    
    // 2. 호스트(Listen Server) 모드로 맵 열기
    // IP 접속이 필요한 경우(클라이언트)는 "127.0.0.1" 등을 URL로 써야 하지만, 
    // 현재는 'Start Game' = 'Create Host' 개념으로 처리합니다.
    // 기존 ?listen 옵션을 포함하여 맵을 엽니다.
    
    FString MapURL = TEXT("/Game/Non/Maps/LV_StartMap?listen");

    UGameplayStatics::OpenLevel(this, FName(*MapURL), true, Options);
}

void UCharacterSelectWidget::OnClickDelete()
{
	if (SelectedSlotIndex < 0) return;

	FString SlotName = FString::Printf(TEXT("Slot%d"), SelectedSlotIndex);
    if (UNonGameInstance* GI = Cast<UNonGameInstance>(GetGameInstance()))
    {
        SlotName = GI->GetSaveSlotName(SelectedSlotIndex);
    }
	
	// 세이브 파일 삭제
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}

	// 선택 초기화 및 UI 갱신
	SelectedSlotIndex = -1;
	if (Btn_StartGame) Btn_StartGame->SetIsEnabled(false);
	if (Btn_Delete) Btn_Delete->SetIsEnabled(false);

	// 보더 초기화
	if (Border_Slot_0) Border_Slot_0->SetVisibility(ESlateVisibility::Hidden);
	if (Border_Slot_1) Border_Slot_1->SetVisibility(ESlateVisibility::Hidden);
	if (Border_Slot_2) Border_Slot_2->SetVisibility(ESlateVisibility::Hidden);

	UpdateSlotDisplay(); // 텍스트 갱신 (-> 캐릭터 생성)
	RefreshLobbyCharacters(); // 3D 캐릭터 숨기기
}

void UCharacterSelectWidget::RefreshLobbyCharacters()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// [New] ItemDataTable 참조 획득 (장비 메쉬 로드용)
	UDataTable* ItemDT = nullptr;
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (UInventoryComponent* Inv = Pawn->FindComponentByClass<UInventoryComponent>())
			{
				ItemDT = Inv->ItemDataTable;
			}
		}
	}

	// 슬롯 0, 1, 2에 대해 반복
	for (int32 i = 0; i < 3; i++)
	{
		FString SlotName = FString::Printf(TEXT("Slot%d"), i);
        if (UNonGameInstance* GI = Cast<UNonGameInstance>(GetGameInstance()))
        {
            SlotName = GI->GetSaveSlotName(i);
        }

		bool bHasData = UGameplayStatics::DoesSaveGameExist(SlotName, 0);

		// 태그: LobbyChar0, LobbyChar1, LobbyChar2
		FName TagName = *FString::Printf(TEXT("LobbyChar%d"), i);
		
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(World, TagName, FoundActors);

		for (AActor* Actor : FoundActors)
		{
			if (bHasData)
			{
				Actor->SetActorHiddenInGame(false);

				// [Changed] 장비 데이터 적용 (EquipmentComponent 직접 호출)
				if (ANonCharacterBase* LobbyChar = Cast<ANonCharacterBase>(Actor))
				{
					if (UEquipmentComponent* EqComp = LobbyChar->FindComponentByClass<UEquipmentComponent>())
					{
						if (UNonSaveGame* SaveData = Cast<UNonSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
						{
							EqComp->RestoreEquippedItemsFromSave(SaveData->EquippedItems);
						}
					}
				}
			}
			else
			{
				Actor->SetActorHiddenInGame(true);
			}
		}
	}
}
