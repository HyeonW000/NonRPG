#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LobbyGameMode.generated.h"

/**
 * 캐릭터 생성(로비) 전용 게임 모드
 * - 캐릭터 자동 소환 방지
 * - 로비 전용 컨트롤러 사용
 */
UCLASS()
class NON_API ALobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	ALobbyGameMode();
};
