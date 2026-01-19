// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NonGameMode.generated.h"

UCLASS(minimalapi)
class ANonGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANonGameMode();
	virtual void PostLogin(APlayerController* NewPlayer) override;
    
    // [New] 접속 시 URL 옵션 파싱 (예: ?Slot=1)
    virtual APlayerController* Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    
    // [New] 클라이언트 요청에 따라 캐릭터 스폰 및 데이터 로드
    void SpawnPlayerFromSave(APlayerController* PC, int32 SlotIndex);

    // [New] 자동 스폰 방지
    // virtual void RestartPlayer(AController* NewPlayer) override; // 필요시 오버라이드
};



