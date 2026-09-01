#include "UI/Chat/ChatMessageEntryWidget.h"
#include "Components/TextBlock.h"

void UChatMessageEntryWidget::InitMessage(const FChatMessage& InMsg)
{
    MessageData = InMsg;

    const FLinearColor ChannelColor = GetChannelColor(InMsg.Channel);
    const FText ChannelPrefix = GetChannelPrefixText(InMsg.Channel);

    // 1) Text_FullMessage 하나로 통째로 표시하는 경우
    if (Text_FullMessage)
    {
        FString FormattedStr;
        if (InMsg.Channel == EChatChannel::System || InMsg.Channel == EChatChannel::Notice)
        {
            FormattedStr = FString::Printf(TEXT("%s %s"), *ChannelPrefix.ToString(), *InMsg.Message);
        }
        else
        {
            // [일반] [버서커_10]: 안녕하세요 (딱 1칸 간격으로 정갈하게 조절)
            FormattedStr = FString::Printf(TEXT("%s [%s]: %s"), *ChannelPrefix.ToString(), *InMsg.SenderName, *InMsg.Message);
        }

        Text_FullMessage->SetText(FText::FromString(FormattedStr));
        Text_FullMessage->SetColorAndOpacity(FSlateColor(ChannelColor));
    }

    // 2) 개별 TextBlock으로 분리되어 있는 경우
    if (Text_Channel)
    {
        Text_Channel->SetText(ChannelPrefix);
        Text_Channel->SetColorAndOpacity(FSlateColor(ChannelColor));
    }

    if (Text_Sender)
    {
        Text_Sender->SetText(FText::FromString(FString::Printf(TEXT("[%s]:"), *InMsg.SenderName)));
        Text_Sender->SetColorAndOpacity(FSlateColor(ChannelColor));
    }

    if (Text_Content)
    {
        Text_Content->SetText(FText::FromString(InMsg.Message));
        // 일반 채팅은 본문 흰색, 그 외는 채널 색상
        if (InMsg.Channel == EChatChannel::General)
        {
            Text_Content->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        }
        else
        {
            Text_Content->SetColorAndOpacity(FSlateColor(ChannelColor));
        }
    }
}

FLinearColor UChatMessageEntryWidget::GetChannelColor(EChatChannel Channel) const
{
    switch (Channel)
    {
    case EChatChannel::General: return GeneralColor;
    case EChatChannel::Party:   return PartyColor;
    case EChatChannel::Guild:   return GuildColor;
    case EChatChannel::Whisper: return WhisperColor;
    case EChatChannel::System:  return SystemColor;
    case EChatChannel::Notice:  return NoticeColor;
    default:                    return GeneralColor;
    }
}

FText UChatMessageEntryWidget::GetChannelPrefixText(EChatChannel Channel) const
{
    switch (Channel)
    {
    case EChatChannel::General:
        return FText::FromString(TEXT("[일반]"));
    case EChatChannel::Party:
        return FText::FromString(TEXT("[파티]"));
    case EChatChannel::Guild:
        return FText::FromString(TEXT("[길드]"));
    case EChatChannel::Whisper:
        return FText::FromString(TEXT("[귓속말]"));
    case EChatChannel::System:
        return FText::FromString(TEXT("[시스템]"));
    case EChatChannel::Notice:
        return FText::FromString(TEXT("[공지]"));
    default:
        return FText::FromString(TEXT("[전체]"));
    }
}
