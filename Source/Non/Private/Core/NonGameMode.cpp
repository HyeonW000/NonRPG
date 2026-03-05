// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/NonGameMode.h"
#include "Character/NonCharacterBase.h"
#include "Core/NonPlayerController.h" // [New]
#include "Kismet/GameplayStatics.h"
#include "System/NonGameInstance.h"
#include "System/NonSaveGame.h"
#include "UObject/ConstructorHelpers.h"

ANonGameMode::ANonGameMode() {
  // set default pawn class to our Blueprinted character
  static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(
      TEXT("/Game/Non/Blueprints/Character/BP_NonCharacterBase"));
  if (PlayerPawnBPClass.Class != NULL) {
    // [Changed] 자동 스폰 대신, 나중에 스폰할 때 쓰기 위해 저장만 해둠
    DefaultPawnClass = PlayerPawnBPClass.Class;
  }

  // [New] 처음 접속 시 바로 스폰하지 않고 대기 (UI에서 요청 시 스폰)
  bStartPlayersAsSpectators = true;

  // [New] PlayerController 클래스를 기본값으로 지정 (Blueprint에서 덮어쓸 수
  // 있음)
  PlayerControllerClass = ANonPlayerController::StaticClass();
}

void ANonGameMode::PostLogin(APlayerController *NewPlayer) {
  Super::PostLogin(NewPlayer);

  // [Changed] 맵 이동 후 자동 스폰 처리
  // 로비 맵이 아닌 경우에만 동작해야 함 (Lobby 맵 이름은 "LV_Lobby"라고 가정)

  UWorld *World = GetWorld();
  if (!World)
    return;

  FString MapName = World->GetMapName();
  MapName.RemoveFromStart(World->StreamingLevelsPrefix); // PIE 접두사 제거

  // 로비가 아니면 스폰 시도
  if (!MapName.Contains(TEXT("Lobby")) && !MapName.Contains(TEXT("Title"))) {
    int32 TargetSlotIndex = -1;

    // 1. 컨트롤러에 저장된 슬롯 정보 확인 (Login 단계에서 파싱됨)
    if (ANonPlayerController *NonPC = Cast<ANonPlayerController>(NewPlayer)) {
      if (NonPC->SelectedSlotIndex >= 0) {
        TargetSlotIndex = NonPC->SelectedSlotIndex;
      }
    }

    // 2. 컨트롤러에 정보가 없다면 GameInstance 확인 (PIE 싱글 테스트 등)
    if (TargetSlotIndex < 0) {
      if (UNonGameInstance *GI = Cast<UNonGameInstance>(GetGameInstance())) {
        if (GI->CurrentSlotIndex >= 0) {
          TargetSlotIndex = GI->CurrentSlotIndex;
        }
      }
    }

    // 3. 스폰 실행
    if (TargetSlotIndex >= 0) {
      SpawnPlayerFromSave(NewPlayer, TargetSlotIndex);
    }
  }
}

void ANonGameMode::SpawnPlayerFromSave(APlayerController *PC, int32 SlotIndex) {
  if (!PC)
    return;

  // [Changed] GameInstance를 통해 PIE 분리된 슬롯 이름 가져오기
  FString SlotName = FString::Printf(TEXT("Slot%d"), SlotIndex);
  if (UNonGameInstance *GI = Cast<UNonGameInstance>(GetGameInstance())) {
    SlotName = GI->GetSaveSlotName(SlotIndex);
  }

  // 1. 세이브 파일 로드
  UNonSaveGame *Data = nullptr;
  if (UGameplayStatics::DoesSaveGameExist(SlotName, 0)) {
    Data = Cast<UNonSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
  }

  if (!Data) {
  }

  // 2. 캐릭터 스폰
  // 현재 Transform (StartPoint) 찾기
  AActor *StartSpot = FindPlayerStart(PC);
  FTransform SpawnTransform =
      StartSpot ? StartSpot->GetActorTransform() : FTransform::Identity;

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

  // DefaultPawnClass (BP_NonCharacterBase) 사용
  UClass *PawnClass = DefaultPawnClass;
  if (!PawnClass)
    PawnClass = ANonCharacterBase::StaticClass();

  if (APawn *NewPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform,
                                                     SpawnParams)) {
    // 3. 빙의 (Spectator -> Character)
    PC->UnPossess();
    PC->Possess(NewPawn);

    // [New] 클라이언트에게 스폰 완료 알림 (UI 제거 및 입력 모드 전환)
    if (ANonPlayerController *NonPC = Cast<ANonPlayerController>(PC)) {
      NonPC->ClientOnSpawnFinished();
    }

    // 4. 데이터 적용 (이름 및 직업)
    if (ANonCharacterBase *Char = Cast<ANonCharacterBase>(NewPawn)) {
      if (Data) {
        Char->InitCharacterData(Data->JobClass, Data->Level);
        Char->SetPlayerName(Data->PlayerName);
      } else {
        // 신규 캐릭터 기본값: 블루프린트에 설정된 DefaultJobClass를 우선 사용
        EJobClass StartJob = EJobClass::Defender;
        if (Char->DefaultJobClass != EJobClass::None) {
          StartJob = Char->DefaultJobClass;
        }

        Char->InitCharacterData(StartJob, 1);
        Char->SetPlayerName(TEXT("New Player"));
      }
    }
  }
}

// [New] 접속 시 URL 옵션 파싱
APlayerController *
ANonGameMode::Login(UPlayer *NewPlayer, ENetRole InRemoteRole,
                    const FString &Portal, const FString &Options,
                    const FUniqueNetIdRepl &UniqueId, FString &ErrorMessage) {
  // 부모 Login 호출 (플레이어 컨트롤러 생성됨)
  APlayerController *PC = Super::Login(NewPlayer, InRemoteRole, Portal, Options,
                                       UniqueId, ErrorMessage);

  if (PC) {
    // URL 옵션에서 "Slot" 값 파싱
    FString SlotStr = UGameplayStatics::ParseOption(Options, TEXT("Slot"));
    if (!SlotStr.IsEmpty() && SlotStr.IsNumeric()) {
      int32 SlotIndex = FCString::Atoi(*SlotStr);

      // NonPlayerController라면 슬롯 정보 저장
      if (ANonPlayerController *NonPC = Cast<ANonPlayerController>(PC)) {
        // SelectedSlotIndex는 Rep Notify가 있을 수 있으므로 서버에서 설정하면
        // 복제됨
        NonPC->SelectedSlotIndex = SlotIndex;
      }
    }
  }

  return PC;
}
