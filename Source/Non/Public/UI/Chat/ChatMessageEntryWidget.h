#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Chat/ChatTypes.h"
#include "ChatMessageEntryWidget.generated.h"

class UTextBlock;
class URichTextBlock;

/**
 * 🌟 단일 채팅 메시지 한 줄을 표시하는 위젯
 */
UCLASS()
class NON_API UChatMessageEntryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 메시지 데이터로 위젯 내용 초기화 */
    UFUNCTION(BlueprintCallable, Category = "Chat")
    void InitMessage(const FChatMessage& InMsg);

    const FChatMessage& GetMessageData() const { return MessageData; }

    /** ── 🎨 [채널별 글자 색상 설정 (에디터 디테일 패널에서 수정 가능)] ── */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|Colors")
    FLinearColor GeneralColor = FLinearColor(0.9f, 0.9f, 0.9f, 1.0f); // 일반: 부드러운 흰색

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|Colors")
    FLinearColor PartyColor = FLinearColor(0.2f, 0.85f, 1.0f, 1.0f); // 파티: 청록색(Cyan)

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|Colors")
    FLinearColor GuildColor = FLinearColor(0.3f, 1.0f, 0.4f, 1.0f);  // 길드: 연두색(Green)

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|Colors")
    FLinearColor WhisperColor = FLinearColor(1.0f, 0.45f, 0.85f, 1.0f); // 귓속말: 분홍색(Pink)

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|Colors")
    FLinearColor SystemColor = FLinearColor(1.0f, 0.8f, 0.2f, 1.0f); // 시스템: 노란색(Yellow)

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|Colors")
    FLinearColor NoticeColor = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f); // 공지: 주황/붉은색

protected:
    /** 전체 완성된 메시지 텍스트 (예: [일반] [버서커_1]: 안녕하세요!) */
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* Text_FullMessage = nullptr;

    /** 채널 태그 (예: [일반], [파티]) */
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* Text_Channel = nullptr;

    /** 보낸 사람 (예: [버서커_1]) */
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* Text_Sender = nullptr;

    /** 메시지 내용 (예: 안녕하세요!) */
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* Text_Content = nullptr;

    /** 채널별 기본 텍스트 색상 반환 */
    UFUNCTION(BlueprintPure, Category = "Chat")
    FLinearColor GetChannelColor(EChatChannel Channel) const;

    /** 채널별 접두사 텍스트 반환 */
    UFUNCTION(BlueprintPure, Category = "Chat")
    FText GetChannelPrefixText(EChatChannel Channel) const;

private:
    FChatMessage MessageData;
};
