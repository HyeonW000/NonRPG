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
    // 캐릭터를 골라 게임에 들어갈 준비가 됐으면 0 (캐릭터는 하나라 0만 씀), 아니면 -1
    UPROPERTY(BlueprintReadWrite, Category = "GameData")
    int32 CurrentSlotIndex = -1;

    /** 캐릭터 세이브 파일 이름. 캐릭터는 하나만 저장한다 (2026-11-28, NON-DIS-14).
     *  저장·불러오기·삭제는 모두 이 이름을 쓴다. PIE에서는 창마다 따로 ("PIE_1_Slot0") */
    UFUNCTION(BlueprintPure, Category = "GameData")
    FString GetCharacterSaveSlotName() const;

private:
    /** PIE 창 번호를 붙인 세이브 이름 (옛 세이브와 같은 "Slot0" 이름을 유지하려고 남김) */
    FString GetSaveSlotName(int32 SlotIndex) const;

public:
    // [New] 로비 진입 시 타이틀 화면 스킵 여부 (인게임 -> 로비 이동 시 true)
    UPROPERTY(BlueprintReadWrite, Category = "GameData")
    bool bSkipTitle = false;

    // 마지막으로 접속했던 허브(마을) 서버 주소 "host:port". 캐릭터 선택 후 시작할 때 여기로 재접속.
    // 비어 있으면 자기 서버를 연다(?listen). 나중에 실제 서버 주소/매칭으로 교체.
    UPROPERTY(BlueprintReadWrite, Category = "GameData")
    FString HubServerAddress;
};
