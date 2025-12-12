#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LobbyPlayerController.generated.h"

/**
 * 로비(캐릭터 생성 화면) 전용 플레이어 컨트롤러
 * - 마우스 커서 표시
 * - UI 입력 모드 설정
 */
UCLASS()
class NON_API ALobbyPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	ALobbyPlayerController();

protected:
	virtual void BeginPlay() override;
};
