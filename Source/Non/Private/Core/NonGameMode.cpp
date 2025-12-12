// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/NonGameMode.h"
#include "Core/NonPlayerController.h" // [New]
#include "Character/NonCharacterBase.h"
#include "UObject/ConstructorHelpers.h"
#include "System/NonGameInstance.h"
#include "System/NonSaveGame.h"
#include "Kismet/GameplayStatics.h"


ANonGameMode::ANonGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/Non/Blueprints/Character/BP_NonCharacterBase"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	// [New] PlayerController 클래스를 기본값으로 지정 (Blueprint에서 덮어쓸 수 있음)
	PlayerControllerClass = ANonPlayerController::StaticClass();
}

void ANonGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 1. GameInstance에서 선택된 슬롯 이름 가져오기
	FString SlotName;
	if (UNonGameInstance* GI = Cast<UNonGameInstance>(GetGameInstance()))
	{
		SlotName = GI->CurrentSlotName;
	}

	// [New] 입력 모드를 게임 모드로 강제 설정
	if (NewPlayer)
	{
		NewPlayer->SetInputMode(FInputModeGameOnly());
		NewPlayer->bShowMouseCursor = false;
	}

	if (SlotName.IsEmpty()) return; // 로비 안 거치고 바로 시작했거나 에러

	// 2. 세이브 파일 로드
	if (UNonSaveGame* Data = Cast<UNonSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
	{
		// 3. 플레이어 캐릭터 찾아서 데이터 적용
		if (NewPlayer)
		{
			if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(NewPlayer->GetPawn()))
			{
				// 직업 및 레벨 초기화
				Char->InitCharacterData(Data->JobClass, Data->Level);
				
				// 이름 설정 (필요하다면 Characters에 저장공간 마련해야 함)
				// Char->SetPlayerName(Data->PlayerName); 
				
				UE_LOG(LogTemp, Log, TEXT("[NonGameMode] Loaded Character: %s Job=%d Level=%d"), *Data->PlayerName, (int32)Data->JobClass, Data->Level);
			}
		}
	}
}

