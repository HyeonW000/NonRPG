#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "System/NonSaveGame.h"
#include "SaveGameSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameSaved, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameLoaded, bool, bSuccess);

/**
 * 게임 저장 및 로드를 관리하는 서브시스템
 */
UCLASS()
class NON_API USaveGameSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 저장 슬롯 이름 (하나만 사용)
    static const FString DefaultSlotName;

    /** 현재 게임 상태를 파일에 저장 */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    void SaveGame();

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
};
