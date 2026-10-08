#pragma once

#include "CoreMinimal.h"
#include "TradeTypes.generated.h"

class ANonPlayerController;
class UTexture2D;

UENUM(BlueprintType)
enum class ETradeState : uint8
{
    None        UMETA(DisplayName = "None"),
    Requested   UMETA(DisplayName = "Requested"),
    Trading     UMETA(DisplayName = "Trading"),
    Completed   UMETA(DisplayName = "Completed"),
    Cancelled   UMETA(DisplayName = "Cancelled")
};

/** 거래창에 등록된 아이템 1칸 정보 */
USTRUCT(BlueprintType)
struct NON_API FTradeItemSlot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Trade")
    int32 TradeSlotIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "Trade")
    int32 SourceInvenSlotIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "Trade")
    FName ItemId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Trade")
    int32 Quantity = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Trade")
    FText ItemName;

    UPROPERTY(BlueprintReadOnly, Category = "Trade")
    TObjectPtr<UTexture2D> ItemIcon = nullptr;

    bool IsValidSlot() const
    {
        return !ItemId.IsNone() && Quantity > 0;
    }
};

/** 거래 참여자의 거래 상태 데이터 */
USTRUCT(BlueprintType)
struct NON_API FTradePlayerData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Trade")
    TWeakObjectPtr<ANonPlayerController> Controller = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Trade")
    FString Nickname;

    UPROPERTY(BlueprintReadOnly, Category = "Trade")
    TArray<FTradeItemSlot> OfferedItems;

    UPROPERTY(BlueprintReadOnly, Category = "Trade")
    int32 OfferedGold = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Trade")
    bool bIsLocked = false;

    UPROPERTY(BlueprintReadOnly, Category = "Trade")
    bool bIsAccepted = false;
};

// UI 연동용 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTradeRequestReceived, const FString&, RequesterNickname, ANonPlayerController*, RequesterPC);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTradeStarted, const FString&, PartnerNickname);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnTradeItemsUpdated, const TArray<FTradeItemSlot>&, MyItems, int32, MyGold, const TArray<FTradeItemSlot>&, PartnerItems, int32, PartnerGold);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnTradeLockStateChanged, bool, bMyLocked, bool, bPartnerLocked, bool, bMyAccepted, bool, bPartnerAccepted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTradeEnded, bool, bSuccess, const FString&, Message);
