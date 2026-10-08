#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "System/TradeTypes.h"
#include "TradeComponent.generated.h"

class ANonPlayerController;
class ANonCharacterBase;
class UInventoryComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class NON_API UTradeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTradeComponent();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── 델리게이트 (UI 연동용) ──
    UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
    FOnTradeRequestReceived OnTradeRequestReceived;

    UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
    FOnTradeStarted OnTradeStarted;

    UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
    FOnTradeItemsUpdated OnTradeItemsUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
    FOnTradeLockStateChanged OnTradeLockStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
    FOnTradeEnded OnTradeEnded;

    // ── 상태 조회 ──
    UFUNCTION(BlueprintPure, Category = "Trade")
    bool IsTrading() const { return CurrentTradeState == ETradeState::Trading; }

    UFUNCTION(BlueprintPure, Category = "Trade")
    ETradeState GetTradeState() const { return CurrentTradeState; }

    UFUNCTION(BlueprintPure, Category = "Trade")
    ANonPlayerController* GetTradePartner() const { return TradePartnerPC.Get(); }

    // ── 클라이언트 명령 (블루프린트 호출 가능) ──
    /** 다른 플레이어에게 1:1 개인 거래 신청 */
    UFUNCTION(BlueprintCallable, Category = "Trade")
    void RequestTrade(ANonPlayerController* TargetPC);

    /** 대상 캐릭터 액터로 1:1 개인 거래 신청 (클라이언트에서도 안전하게 작동) */
    UFUNCTION(BlueprintCallable, Category = "Trade")
    void RequestTradeByCharacter(ANonCharacterBase* TargetChar);

    /** 거래 신청 응답 (수락 / 거절) */
    UFUNCTION(BlueprintCallable, Category = "Trade")
    void RespondTradeRequest(bool bAccept);

    void SetPendingRequesterPC(ANonPlayerController* InPC) { PendingRequesterPC = InPC; }

    /** 인벤토리 아이템을 거래 슬롯에 올리기 */
    UFUNCTION(BlueprintCallable, Category = "Trade")
    void OfferItem(int32 InvenSlotIndex, int32 TradeSlotIndex);

    /** 올린 아이템 회수 */
    UFUNCTION(BlueprintCallable, Category = "Trade")
    void RemoveOfferedItem(int32 TradeSlotIndex);

    /** 거래 골드 입력 */
    UFUNCTION(BlueprintCallable, Category = "Trade")
    void OfferGold(int32 Amount);

    /** 거래 잠금 (아이템/골드 수정 완료 및 잠금) */
    UFUNCTION(BlueprintCallable, Category = "Trade")
    void SetTradeLock(bool bLock);

    /** 최종 거래 확정 */
    UFUNCTION(BlueprintCallable, Category = "Trade")
    void ConfirmTrade();

    /** 거래 취소 (창 닫기 등) */
    UFUNCTION(BlueprintCallable, Category = "Trade")
    void CancelTrade(const FString& Reason = TEXT("거래가 취소되었습니다."));

    // ── 네트워크 RPC ──
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestTrade(ANonPlayerController* TargetPC);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestTradeByCharacter(ANonCharacterBase* TargetChar);

    UFUNCTION(Client, Reliable)
    void Client_ReceiveTradeRequest(const FString& RequesterNickname, ANonPlayerController* RequesterPC);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RespondTradeRequest(bool bAccept);

    UFUNCTION(Client, Reliable)
    void Client_StartTrade(const FString& PartnerNickname);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_OfferItem(int32 InvenSlotIndex, int32 TradeSlotIndex);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RemoveOfferedItem(int32 TradeSlotIndex);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_OfferGold(int32 Amount);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_SetTradeLock(bool bLock);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ConfirmTrade();

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_CancelTrade(const FString& Reason);

    /** 거래 화면 및 슬롯 정보 동기화 RPC */
    UFUNCTION(Client, Reliable)
    void Client_SyncTrade(const TArray<FTradeItemSlot>& MyItems, int32 MyGold,
                          const TArray<FTradeItemSlot>& PartnerItems, int32 PartnerGold,
                          bool bInMyLocked, bool bInPartnerLocked,
                          bool bInMyAccepted, bool bInPartnerAccepted);

    /** 거래 종료 알림 */
    UFUNCTION(Client, Reliable)
    void Client_TradeEnded(bool bSuccess, const FString& Message);

protected:
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Trade")
    ETradeState CurrentTradeState = ETradeState::None;

    UPROPERTY()
    TWeakObjectPtr<ANonPlayerController> TradePartnerPC = nullptr;

    UPROPERTY()
    TWeakObjectPtr<ANonPlayerController> PendingRequesterPC = nullptr;

    // 내가 올린 거래 아이템 목록 (최대 8개)
    UPROPERTY()
    TArray<FTradeItemSlot> MyOfferedItems;

    UPROPERTY()
    int32 MyOfferedGold = 0;

    UPROPERTY()
    bool bMyLocked = false;

    UPROPERTY()
    bool bMyAccepted = false;

    static const int32 MaxTradeSlots = 8;

    void ResetTradeData();
    void PushTradeSyncToBoth();
    void ExecuteServerTradeSwap();
};
