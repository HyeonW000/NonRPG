#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "System/NonSaveGame.h"
#include "SaveGameSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameSaved, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameLoaded, bool, bSuccess);

class ANonCharacterBase;

/**
 * 캐릭터 저장 (각자 PC에서, 자기 캐릭터만).
 *  - 자동 저장: 3초마다 내용을 비교해서 바뀌었을 때만 쓴다 (위치·HP·MP만 바뀐 건 저장 안 함)
 *  - 곧 저장: 레벨업(서버 요청)·거래 완료 등 -> 복제가 도착할 시간을 두고 저장
 *  - 즉시 저장: 맵 이동·ESC 메뉴·종료 (SaveGame), 캐릭터가 사라질 때 (SaveCharacter)
 *  - 백업: 맵에 들어와 처음 쓸 때 직전 파일을 "_Backup"으로 남기고, 본 파일이 깨졌으면 백업에서 읽는다
 *  - 들어와서 세이브 적용이 끝나기 전(SetSaveReady 전)에는 저장하지 않는다 (빈 상태로 덮어쓰기 방지)
 */
UCLASS()
class NON_API USaveGameSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 지금 바로 저장 (맵 이동·ESC 메뉴·종료 전) */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    void SaveGame();

    /** 이 캐릭터로 바로 저장 (캐릭터 EndPlay처럼 컨트롤러에서 떨어지는 중일 때) */
    void SaveCharacter(ANonCharacterBase* Char);

    /** 잠시 뒤 저장. 서버에서 바뀐 값이 복제로 도착할 시간을 둔다 */
    void RequestSaveSoon(float DelaySeconds = 0.5f);

    /** 캐릭터 세이브 읽기. 본 파일이 깨졌으면 백업에서 읽는다. 없으면 nullptr */
    UNonSaveGame* LoadCharacterSave() const;

    /** 이번 맵에서 세이브 적용이 끝났는지. 끝나기 전에는 어떤 저장도 하지 않는다 */
    void SetSaveReady(bool bReady);
    bool IsSaveReady() const { return bSaveReady; }

    /** 파일에서 게임 상태를 로드하여 적용 */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    void LoadGame();

    /** 저장된 게임 데이터를 삭제 (초기화) */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    void DeleteSaveGame();

    UPROPERTY(BlueprintAssignable)
    FOnGameSaved OnGameSaved;

    UPROPERTY(BlueprintAssignable)
    FOnGameLoaded OnGameLoaded;

private:
    bool AutoSaveTick(float DeltaTime);
    void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

    /** 캐릭터 상태를 세이브 객체에 채운다 */
    void FillSaveData(UNonSaveGame* SaveInst, ANonCharacterBase* Char) const;

    /** 저장 (bOnlyIfChanged면 내용이 같을 때 건너뜀). 썼으면 true */
    bool WriteSave(ANonCharacterBase* Char, bool bOnlyIfChanged);

    /** 바뀜 비교용 바이트 (위치·HP·MP는 빼고) */
    TArray<uint8> MakeCompareBytes(UNonSaveGame* SaveInst) const;

    ANonCharacterBase* GetLocalCharacter() const;
    FString GetSlotName() const;
    static FString GetBackupSlotName(const FString& Slot) { return Slot + TEXT("_Backup"); }

    /** 파일에서 한 번 읽어 두고 계속 채워 쓰는 세이브 (이 함수가 다루지 않는 칸 — 개인 월드 기록 등 — 을 보존) */
    UPROPERTY()
    TObjectPtr<UNonSaveGame> CachedSave;

    FTSTicker::FDelegateHandle TickHandle;
    TArray<uint8> LastCompareBytes;
    bool bSaveReady = false;
    bool bBackupDone = false;
    double SaveSoonAt = -1.0;
    double NextAutoCheckAt = 0.0;

    static constexpr float TickInterval = 0.5f;
    static constexpr float AutoSaveInterval = 3.f;
};
