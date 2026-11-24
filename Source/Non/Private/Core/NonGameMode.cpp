// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/NonGameMode.h"
#include "Character/NonCharacterBase.h"
#include "Core/NonPlayerController.h" // [New]
#include "Kismet/GameplayStatics.h"
#include "System/NonGameInstance.h"
#include "System/NonSaveGame.h"

ANonGameMode::ANonGameMode() {
  // 기본 폰 클래스는 GM_NonGameMode(BP)에서 지정

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

    // 3. 스폰 실행: 세이브는 각 플레이어 PC에만 있으므로 서버가 대신 읽을 수 있는 건 로컬 플레이어(호스트)뿐.
    //    원격 클라이언트는 접속 후 자기 세이브를 읽어 ServerSpawnCharacter로 직접 요청한다.
    if (TargetSlotIndex >= 0 && NewPlayer && NewPlayer->IsLocalController()) {
      SpawnPlayerFromSave(NewPlayer, TargetSlotIndex);
    }
  }
}

void ANonGameMode::SpawnPlayerFromSave(APlayerController *PC, int32 SlotIndex) {
  if (!PC)
    return;

  FNonSpawnCharacterData Data;
  if (ANonPlayerController *NonPC = Cast<ANonPlayerController>(PC)) {
    NonPC->LoadLocalSpawnData(SlotIndex, Data);
  }
  SpawnPlayerWithData(PC, Data);
}

void ANonGameMode::SpawnPlayerWithData(APlayerController *PC, const FNonSpawnCharacterData &Data) {
  if (!PC)
    return;

  // 이미 살아 있는 캐릭터가 있으면 중복 스폰하지 않음
  if (ANonCharacterBase *Existing = Cast<ANonCharacterBase>(PC->GetPawn())) {
    if (!Existing->IsDead())
      return;
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
      UE_LOG(LogTemp, Log, TEXT("[Spawn] %s: save=%d name=%s job=%d level=%d"),
             *GetNameSafe(PC), Data.bHasSave ? 1 : 0, *Data.PlayerName, (int32)Data.JobClass, Data.Level);

      if (Data.bHasSave && !Data.PlayerName.IsEmpty()) {
        // 클라이언트가 보낸 값 정리 (잘못된 직업/레벨 방어)
        EJobClass Job = Data.JobClass;
        if (Job == EJobClass::None || (uint8)Job > (uint8)EJobClass::Sorcerer) {
          Job = (Char->DefaultJobClass != EJobClass::None) ? Char->DefaultJobClass : EJobClass::Defender;
        }
        const int32 Level = FMath::Max(1, Data.Level);

        Char->InitCharacterData(Job, Level);
        Char->SetPlayerName(Data.PlayerName);
        if (ANonPlayerController* NonPC = Cast<ANonPlayerController>(PC)) {
          NonPC->SetPlayerNickname(Data.PlayerName);
        }
      } else {
        // 신규 캐릭터 기본값: 블루프린트에 설정된 DefaultJobClass를 우선 사용
        EJobClass StartJob = EJobClass::Defender;
        if (Char->DefaultJobClass != EJobClass::None) {
          StartJob = Char->DefaultJobClass;
        }

        Char->InitCharacterData(StartJob, 1);

        FString SpawnName = TEXT("플레이어");
        if (ANonPlayerController* NonPC = Cast<ANonPlayerController>(PC)) {
          SpawnName = NonPC->GetPlayerNickname();
          NonPC->SetPlayerNickname(SpawnName);
        }
        Char->SetPlayerName(SpawnName);
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
