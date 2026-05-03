#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DialogueData.generated.h"

/**
 * 대화 선택지(버튼) 하나에 들어가는 데이터 구조체
 */
USTRUCT(BlueprintType)
struct NON_API FDialogueChoice
{
    GENERATED_BODY()

    // 화면에 보여질 선택지 텍스트 (예: "상점 이용하기")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FText ChoiceText;

    // 이 선택지를 고르면 넘어갈 다음 대화의 Row Name (없으면 비워둠)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FName NextNodeID;

    // 선택 시 발동할 특수 액션 (예: "OpenShop", "EndDialogue" 등)
    // 일반 대화 진행이라면 "None" 유지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FName ActionToTrigger;
};

/**
 * 대화 테이블의 한 줄(Row)을 구성하는 데이터 구조체
 */
USTRUCT(BlueprintType)
struct NON_API FDialogueRow : public FTableRowBase
{
    GENERATED_BODY()

    // 이 대사를 치는 화자의 이름 (비워두면 대상을 클릭한 NPC의 기본 이름 사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FText NPCName;

    // 실제 화면에 출력될 대사
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MultiLine = true), Category = "Dialogue")
    FText DialogueText;

    // 텍스트를 다 읽고 (다음) 버튼을 누르면 넘어갈 다음 대화 Row Name (선택지가 없을 때 사용)
    // 비어있으면 이 대화가 마지막 대화로 간주되어 자동 종료됩니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FName NextNodeID;

    // 이 대사에 도달했을 때 띄울 선택지 버튼 목록
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    TArray<FDialogueChoice> Choices;
};
