#include "Core/LobbyGameMode.h"
#include "Core/LobbyPlayerController.h"

ALobbyGameMode::ALobbyGameMode()
{
	// 로비에서는 캐릭터(Pawn)를 자동 소환하지 않음 (UI만 조작)
	DefaultPawnClass = nullptr;

	// 로비 전용 PC (마우스 커서 On 등)
	PlayerControllerClass = ALobbyPlayerController::StaticClass();
}
