#include "Core/LobbyPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h" // [New] for GetAllActorsWithTag
#include "TimerManager.h" // [New] for Timer
#include "System/NonGameInstance.h" // [New]

ALobbyPlayerController::ALobbyPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ALobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 로컬 플레이어 컨트롤러인 경우에만 UI 띄우기 (서버/다른 클라 제외)
	if (IsLocalController())
	{
		// [Optimization] 자주 쓰는 카메라는 미리 찾아서 캐싱해둔다. (매번 검색 비용 절약)
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), LobbyCameraTag, FoundActors);
		if (FoundActors.Num() > 0) CachedLobbyCameraActor = FoundActors[0];

		FoundActors.Empty();
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), CreationCameraTag, FoundActors);
		if (FoundActors.Num() > 0) CachedCreationCameraActor = FoundActors[0];

		// 초기 화면: Title Widget
		bool bShouldShowTitle = true;
		if (UNonGameInstance* GI = Cast<UNonGameInstance>(GetGameInstance()))
		{
			if (GI->bSkipTitle)
			{
				bShouldShowTitle = false;
				GI->bSkipTitle = false; // 일회성이므로 리셋
			}
		}

		// 1. 타이틀 화면이 있으면 먼저 띄움 (스킵 아닐 때만)
		if (bShouldShowTitle && TitleWidgetClass)
		{
			if (UUserWidget* TitleWidget = CreateWidget<UUserWidget>(this, TitleWidgetClass))
			{
				TitleWidget->AddToViewport();
				CurrentWidget = TitleWidget;

				// 입력 모드: UI Only
				FInputModeUIOnly InputMode;
				InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				SetInputMode(InputMode);
				bShowMouseCursor = true;
			}
		}
		// 2. 타이틀이 없거나 스킵이면 캐릭터 선택창 바로 띄움
		else if (CharacterSelectWidgetClass)
		{
			StartCharacterSelect();
		}

		// 초기 카메라 설정
		SetViewToCameraWithTag(LobbyCameraTag);
	}
}

void ALobbyPlayerController::StartCharacterSelect()
{
	if (!IsLocalController()) return;

	// 기존 위젯(타이틀) 제거
	if (CurrentWidget)
	{
		CurrentWidget->RemoveFromParent();
		CurrentWidget = nullptr;
	}

	// 캐릭터 선택창 생성
	if (CharacterSelectWidgetClass)
	{
		if (UUserWidget* Widget = CreateWidget<UUserWidget>(this, CharacterSelectWidgetClass))
		{
			Widget->AddToViewport();
			CurrentWidget = Widget;
		}
	}
    
	// [New] 카메라 재확인 (혹시 풀렸을 경우 대비, 혹은 다른 카메라로 전환 가능)
	// 로비 카메라로 즉시 복귀
	SetViewToCameraWithTag(LobbyCameraTag, 0.0f);
}

#include "UI/CharacterCreationWidget.h" // [New] 헤더 필요 (Cast 등 위해)

void ALobbyPlayerController::StartCharacterCreation(int32 SlotIndex)
{
	if (!IsLocalController()) return;

	// 기존 위젯(선택창) 제거
	if (CurrentWidget)
	{
		CurrentWidget->RemoveFromParent();
		CurrentWidget = nullptr;
	}

	// 캐릭터 생성창 생성
	if (CharacterCreationWidgetClass)
	{
		if (UUserWidget* Widget = CreateWidget<UUserWidget>(this, CharacterCreationWidgetClass))
		{
			// [Important] 슬롯 번호 전달!
			if (UCharacterCreationWidget* CreationWidget = Cast<UCharacterCreationWidget>(Widget))
			{
				CreationWidget->TargetSlotIndex = SlotIndex;
			}
			
			Widget->AddToViewport();
			CurrentWidget = Widget;
		}
	}

	// [New] 생성용 카메라로 즉시 전환
	SetViewToCameraWithTag(CreationCameraTag, 0.0f);
}

void ALobbyPlayerController::OnCharacterCreationFinished()
{
	// [Fix] UI가 닫히거나 캐릭터가 Destroy되는 과정과 충돌할 수 있으므로, 아주 짧은 딜레이 후 실행
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, [this]()
	{
		SetViewToCameraWithTag(LobbyCameraTag);
		
		// [New] UI도 캐릭터 선택창으로 복귀
		StartCharacterSelect();

		// 혹시 모르니 UI 입력 모드도 재확인
		if (IsLocalController())
		{
			FInputModeUIOnly InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputMode);
			bShowMouseCursor = true;
		}

	}, 0.1f, false);
}

void ALobbyPlayerController::SetViewToCameraWithTag(FName Tag, float BlendTime)
{
	if (Tag.IsNone()) return;

	AActor* TargetActor = nullptr;

	// 1. 캐싱된 카메라 우선 확인
	if (Tag == LobbyCameraTag && CachedLobbyCameraActor)
	{
		TargetActor = CachedLobbyCameraActor;
	}
	else if (Tag == CreationCameraTag && CachedCreationCameraActor)
	{
		TargetActor = CachedCreationCameraActor;
	}
	else
	{
		// 2. 캐싱된 게 없으면 태그로 검색 (Fallback)
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, FoundActors);
		if (FoundActors.Num() > 0)
		{
			TargetActor = FoundActors[0];
		}
	}

	if (TargetActor)
	{
		SetViewTargetWithBlend(TargetActor, BlendTime); 
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyPC] Could not find camera with tag: %s"), *Tag.ToString());
	}
}

void ALobbyPlayerController::ServerStartGame_Implementation(const FString& MapName)
{
    if (UWorld* World = GetWorld())
    {
        // 리슨 서버로 실행하여 연결된 클라이언트들을 데리고 이동
        World->ServerTravel(MapName + TEXT("?listen"));
    }
}
