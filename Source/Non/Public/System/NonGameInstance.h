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
	// 로비에서 선택된 슬롯 이름 (예: "Slot0", PIE에서는 "PIE_0_Slot0")
	UPROPERTY(BlueprintReadWrite, Category = "GameData")
	FString CurrentSlotName;

    // [New] 슬롯 인덱스 (문자열 파싱 오류 방지)
    UPROPERTY(BlueprintReadWrite, Category = "GameData")
    int32 CurrentSlotIndex = -1;

    // [New] PIE 환경을 고려한 세이브 슬롯 이름 생성 헬퍼
    UFUNCTION(BlueprintPure, Category = "GameData")
    FString GetSaveSlotName(int32 SlotIndex) const;
    // [New] 로비 진입 시 타이틀 화면 스킵 여부 (인게임 -> 로비 이동 시 true)
    UPROPERTY(BlueprintReadWrite, Category = "GameData")
    bool bSkipTitle = false;

    // 마지막으로 접속했던 허브(마을) 서버 주소 "host:port". 캐릭터 선택 후 시작할 때 여기로 재접속.
    // 비어 있으면 자기 서버를 연다(?listen). 나중에 실제 서버 주소/매칭으로 교체.
    UPROPERTY(BlueprintReadWrite, Category = "GameData")
    FString HubServerAddress;
};
