#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "NonGameInstance.generated.h"

/**
 * 게임 전반에서 유지되어야 할 데이터를 관리합니다.
 * (예: 로비에서 선택한 캐릭터 슬롯 정보)
 */
UCLASS()
class NON_API UNonGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	// 로비에서 선택된 슬롯 이름 (예: "Slot0")
	UPROPERTY(BlueprintReadWrite, Category = "GameData")
	FString CurrentSlotName;
};
