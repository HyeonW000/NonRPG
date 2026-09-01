#pragma once

#include "CoreMinimal.h"
#include "ChatTypes.generated.h"

/**
 * 🌟 채팅 채널 구분
 */
UENUM(BlueprintType)
enum class EChatChannel : uint8
{
    All         UMETA(DisplayName = "전체"),
    General     UMETA(DisplayName = "일반"),
    Party       UMETA(DisplayName = "파티"),
    Guild       UMETA(DisplayName = "길드"),
    Whisper     UMETA(DisplayName = "귓속말"),
    System      UMETA(DisplayName = "시스템"),
    Notice      UMETA(DisplayName = "공지")
};

/**
 * 🌟 단일 채팅 메시지 데이터 구조체
 */
USTRUCT(BlueprintType)
struct FChatMessage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Chat")
    EChatChannel Channel = EChatChannel::General;

    UPROPERTY(BlueprintReadWrite, Category = "Chat")
    FString SenderName = TEXT("");

    UPROPERTY(BlueprintReadWrite, Category = "Chat")
    FString Message = TEXT("");

    UPROPERTY(BlueprintReadWrite, Category = "Chat")
    FDateTime Timestamp = FDateTime::Now();

    UPROPERTY(BlueprintReadWrite, Category = "Chat")
    FLinearColor CustomColor = FLinearColor::White;

    FChatMessage()
        : Channel(EChatChannel::General)
        , SenderName(TEXT(""))
        , Message(TEXT(""))
        , Timestamp(FDateTime::Now())
        , CustomColor(FLinearColor::White)
    {
    }

    FChatMessage(EChatChannel InChannel, const FString& InSender, const FString& InMsg)
        : Channel(InChannel)
        , SenderName(InSender)
        , Message(InMsg)
        , Timestamp(FDateTime::Now())
        , CustomColor(FLinearColor::White)
    {
    }
};
