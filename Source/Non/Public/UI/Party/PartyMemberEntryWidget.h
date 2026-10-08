#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "System/PartyTypes.h"
#include "PartyMemberEntryWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class UBorder;

/**
 * 파티원 1명의 직업, 닉네임, 체력바, 마나바를 표시하는 개별 슬롯 위젯
 */
UCLASS()
class NON_API UPartyMemberEntryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPartyMemberEntryWidget(const FObjectInitializer& ObjectInitializer);

    /** 파티원 데이터 갱신 */
    void UpdateMemberInfo(const FPartyMemberInfo& InMemberInfo);

protected:
    virtual void NativeConstruct() override;

    /** C++ 코드 기반 위젯 트리 동적 구축 (UMG 블루프린트 없이도 즉시 표시) */
    void BuildCustomWidgetTree();

    /** 직업별 고유 색상 및 명칭 반환 */
    static void GetJobDisplayInfo(EJobClass Job, FString& OutJobName, FLinearColor& OutBadgeColor);

public:
    UPROPERTY(meta = (BindWidgetOptional))
    UBorder* Border_Card = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* Image_JobIcon = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* Text_LeaderBadge = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* Text_PlayerName = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* ProgressBar_HP = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* ProgressBar_MP = nullptr;

private:
    bool bTreeBuilt = false;
};
