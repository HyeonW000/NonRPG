#include "System/TradeComponent.h"
#include "Core/NonPlayerController.h"
#include "Character/NonCharacterBase.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryItem.h"
#include "Net/UnrealNetwork.h"

UTradeComponent::UTradeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UTradeComponent::BeginPlay()
{
    Super::BeginPlay();
    ResetTradeData();
}

void UTradeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UTradeComponent, CurrentTradeState);
}

void UTradeComponent::ResetTradeData()
{
    CurrentTradeState = ETradeState::None;
    TradePartnerPC = nullptr;
    PendingRequesterPC = nullptr;
    MyOfferedItems.Empty();
    MyOfferedGold = 0;
    bMyLocked = false;
    bMyAccepted = false;
}

void UTradeComponent::RequestTrade(ANonPlayerController* TargetPC)
{
    Server_RequestTrade(TargetPC);
}

void UTradeComponent::RequestTradeByCharacter(ANonCharacterBase* TargetChar)
{
    Server_RequestTradeByCharacter(TargetChar);
}

bool UTradeComponent::Server_RequestTradeByCharacter_Validate(ANonCharacterBase* TargetChar)
{
    return true;
}

void UTradeComponent::Server_RequestTradeByCharacter_Implementation(ANonCharacterBase* TargetChar)
{
    if (!TargetChar) return;
    ANonPlayerController* TargetPC = Cast<ANonPlayerController>(TargetChar->GetController());
    if (!TargetPC && GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (ANonPlayerController* NonPC = Cast<ANonPlayerController>(It->Get()))
            {
                if (NonPC->GetPawn() == TargetChar)
                {
                    TargetPC = NonPC;
                    break;
                }
            }
        }
    }

    if (TargetPC)
    {
        Server_RequestTrade(TargetPC);
    }
}

bool UTradeComponent::Server_RequestTrade_Validate(ANonPlayerController* TargetPC)
{
    return true;
}

void UTradeComponent::Server_RequestTrade_Implementation(ANonPlayerController* TargetPC)
{
    ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwner());
    if (!MyPC || !TargetPC || MyPC == TargetPC) return;

    UTradeComponent* TargetTradeComp = TargetPC->GetTradeComponent();
    if (!TargetTradeComp)
    {
        TargetTradeComp = TargetPC->FindComponentByClass<UTradeComponent>();
    }
    if (!TargetTradeComp) return;

    if (IsTrading())
    {
        Client_TradeEnded(false, TEXT("현재 다른 플레이어와 거래 중입니다."));
        return;
    }

    if (TargetTradeComp->IsTrading())
    {
        Client_TradeEnded(false, TargetPC->GetPlayerNickname() + TEXT("님이 다른 플레이어와 거래 중입니다."));
        return;
    }

    TargetTradeComp->PendingRequesterPC = MyPC;
    TargetTradeComp->Client_ReceiveTradeRequest(MyPC->GetPlayerNickname(), MyPC);
}

void UTradeComponent::Client_ReceiveTradeRequest_Implementation(const FString& RequesterNickname, ANonPlayerController* RequesterPC)
{
    PendingRequesterPC = RequesterPC;
    OnTradeRequestReceived.Broadcast(RequesterNickname, RequesterPC);

    if (ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwner()))
    {
        MyPC->ShowInteractionRequest(EInteractionRequestType::TradeRequest, RequesterNickname, RequesterPC, 15.f);
    }
}

void UTradeComponent::RespondTradeRequest(bool bAccept)
{
    Server_RespondTradeRequest(bAccept);
}

bool UTradeComponent::Server_RespondTradeRequest_Validate(bool bAccept)
{
    return true;
}

void UTradeComponent::Server_RespondTradeRequest_Implementation(bool bAccept)
{
    ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwner());
    if (!MyPC || !PendingRequesterPC.IsValid()) return;

    UTradeComponent* RequesterTradeComp = PendingRequesterPC->FindComponentByClass<UTradeComponent>();
    if (!RequesterTradeComp)
    {
        PendingRequesterPC = nullptr;
        return;
    }

    if (!bAccept)
    {
        RequesterTradeComp->Client_TradeEnded(false, MyPC->GetPlayerNickname() + TEXT("님이 거래 요청을 거절했습니다."));
        PendingRequesterPC = nullptr;
        return;
    }

    // 거래 시작 처리
    ResetTradeData();
    RequesterTradeComp->ResetTradeData();

    CurrentTradeState = ETradeState::Trading;
    TradePartnerPC = PendingRequesterPC;

    RequesterTradeComp->CurrentTradeState = ETradeState::Trading;
    RequesterTradeComp->TradePartnerPC = MyPC;

    PendingRequesterPC = nullptr;

    // 양측에 거래 시작 알림
    Client_StartTrade(TradePartnerPC->GetPlayerNickname());
    RequesterTradeComp->Client_StartTrade(MyPC->GetPlayerNickname());

    PushTradeSyncToBoth();
}

void UTradeComponent::Client_StartTrade_Implementation(const FString& PartnerNickname)
{
    CurrentTradeState = ETradeState::Trading;
    OnTradeStarted.Broadcast(PartnerNickname);
}

void UTradeComponent::OfferItem(int32 InvenSlotIndex, int32 TradeSlotIndex)
{
    Server_OfferItem(InvenSlotIndex, TradeSlotIndex);
}

bool UTradeComponent::Server_OfferItem_Validate(int32 InvenSlotIndex, int32 TradeSlotIndex)
{
    return true;
}

void UTradeComponent::Server_OfferItem_Implementation(int32 InvenSlotIndex, int32 TradeSlotIndex)
{
    if (!IsTrading() || bMyLocked) return;

    ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwner());
    if (!MyPC) return;

    ANonCharacterBase* MyChar = Cast<ANonCharacterBase>(MyPC->GetPawn());
    if (!MyChar) return;

    UInventoryComponent* InvenComp = MyChar->FindComponentByClass<UInventoryComponent>();
    if (!InvenComp) return;

    UInventoryItem* Item = InvenComp->GetItemAt(InvenSlotIndex);
    if (!Item) return;

    // 이미 등록된 슬롯인지 확인 후 교체 또는 추가
    FTradeItemSlot NewSlot;
    NewSlot.TradeSlotIndex = TradeSlotIndex;
    NewSlot.SourceInvenSlotIndex = InvenSlotIndex;
    NewSlot.ItemId = Item->ItemId;
    NewSlot.Quantity = Item->Quantity;
    NewSlot.ItemName = Item->CachedRow.Name;
    NewSlot.ItemIcon = Item->GetIcon();

    int32 FoundIdx = INDEX_NONE;
    for (int32 i = 0; i < MyOfferedItems.Num(); ++i)
    {
        if (MyOfferedItems[i].TradeSlotIndex == TradeSlotIndex)
        {
            FoundIdx = i;
            break;
        }
    }

    if (FoundIdx != INDEX_NONE)
    {
        MyOfferedItems[FoundIdx] = NewSlot;
    }
    else
    {
        if (MyOfferedItems.Num() < MaxTradeSlots)
        {
            MyOfferedItems.Add(NewSlot);
        }
    }

    // 거래 내용이 변경되었으므로 양측의 잠금 및 수락 상태를 초기화
    bMyLocked = false;
    bMyAccepted = false;
    if (TradePartnerPC.IsValid())
    {
        if (UTradeComponent* PartnerComp = TradePartnerPC->FindComponentByClass<UTradeComponent>())
        {
            PartnerComp->bMyLocked = false;
            PartnerComp->bMyAccepted = false;
        }
    }

    PushTradeSyncToBoth();
}

void UTradeComponent::RemoveOfferedItem(int32 TradeSlotIndex)
{
    Server_RemoveOfferedItem(TradeSlotIndex);
}

bool UTradeComponent::Server_RemoveOfferedItem_Validate(int32 TradeSlotIndex)
{
    return true;
}

void UTradeComponent::Server_RemoveOfferedItem_Implementation(int32 TradeSlotIndex)
{
    if (!IsTrading() || bMyLocked) return;

    for (int32 i = 0; i < MyOfferedItems.Num(); ++i)
    {
        if (MyOfferedItems[i].TradeSlotIndex == TradeSlotIndex)
        {
            MyOfferedItems.RemoveAt(i);
            break;
        }
    }

    bMyLocked = false;
    bMyAccepted = false;
    if (TradePartnerPC.IsValid())
    {
        if (UTradeComponent* PartnerComp = TradePartnerPC->FindComponentByClass<UTradeComponent>())
        {
            PartnerComp->bMyLocked = false;
            PartnerComp->bMyAccepted = false;
        }
    }

    PushTradeSyncToBoth();
}

void UTradeComponent::OfferGold(int32 Amount)
{
    Server_OfferGold(Amount);
}

bool UTradeComponent::Server_OfferGold_Validate(int32 Amount)
{
    return true;
}

void UTradeComponent::Server_OfferGold_Implementation(int32 Amount)
{
    if (!IsTrading() || bMyLocked || Amount < 0) return;

    ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwner());
    if (!MyPC) return;

    ANonCharacterBase* MyChar = Cast<ANonCharacterBase>(MyPC->GetPawn());
    if (!MyChar) return;

    UInventoryComponent* InvenComp = MyChar->FindComponentByClass<UInventoryComponent>();
    if (!InvenComp) return;

    if (InvenComp->GetGold() < Amount) return;

    MyOfferedGold = Amount;

    bMyLocked = false;
    bMyAccepted = false;
    if (TradePartnerPC.IsValid())
    {
        if (UTradeComponent* PartnerComp = TradePartnerPC->FindComponentByClass<UTradeComponent>())
        {
            PartnerComp->bMyLocked = false;
            PartnerComp->bMyAccepted = false;
        }
    }

    PushTradeSyncToBoth();
}

void UTradeComponent::SetTradeLock(bool bLock)
{
    Server_SetTradeLock(bLock);
}

bool UTradeComponent::Server_SetTradeLock_Validate(bool bLock)
{
    return true;
}

void UTradeComponent::Server_SetTradeLock_Implementation(bool bLock)
{
    if (!IsTrading()) return;

    bMyLocked = bLock;
    if (!bMyLocked)
    {
        bMyAccepted = false;
    }

    PushTradeSyncToBoth();
}

void UTradeComponent::ConfirmTrade()
{
    Server_ConfirmTrade();
}

bool UTradeComponent::Server_ConfirmTrade_Validate()
{
    return true;
}

void UTradeComponent::Server_ConfirmTrade_Implementation()
{
    if (!IsTrading() || !bMyLocked) return;
    if (!TradePartnerPC.IsValid()) return;

    UTradeComponent* PartnerComp = TradePartnerPC->FindComponentByClass<UTradeComponent>();
    if (!PartnerComp || !PartnerComp->bMyLocked) return;

    bMyAccepted = true;

    // 양 플레이어 모두 확정을 눌렀을 때 실제 아이템/골드 교환 실행
    if (bMyAccepted && PartnerComp->bMyAccepted)
    {
        ExecuteServerTradeSwap();
    }
    else
    {
        PushTradeSyncToBoth();
    }
}

void UTradeComponent::CancelTrade(const FString& Reason)
{
    Server_CancelTrade(Reason);
}

bool UTradeComponent::Server_CancelTrade_Validate(const FString& Reason)
{
    return true;
}

void UTradeComponent::Server_CancelTrade_Implementation(const FString& Reason)
{
    if (!IsTrading()) return;

    if (TradePartnerPC.IsValid())
    {
        if (UTradeComponent* PartnerComp = TradePartnerPC->FindComponentByClass<UTradeComponent>())
        {
            PartnerComp->Client_TradeEnded(false, TEXT("상대방이 ") + Reason);
            PartnerComp->ResetTradeData();
        }
    }

    Client_TradeEnded(false, Reason);
    ResetTradeData();
}

void UTradeComponent::PushTradeSyncToBoth()
{
    if (!TradePartnerPC.IsValid()) return;
    UTradeComponent* PartnerComp = TradePartnerPC->FindComponentByClass<UTradeComponent>();
    if (!PartnerComp) return;

    // 내 화면 동기화
    Client_SyncTrade(MyOfferedItems, MyOfferedGold, PartnerComp->MyOfferedItems, PartnerComp->MyOfferedGold,
                     bMyLocked, PartnerComp->bMyLocked, bMyAccepted, PartnerComp->bMyAccepted);

    // 상대방 화면 동기화
    PartnerComp->Client_SyncTrade(PartnerComp->MyOfferedItems, PartnerComp->MyOfferedGold, MyOfferedItems, MyOfferedGold,
                                  PartnerComp->bMyLocked, bMyLocked, PartnerComp->bMyAccepted, bMyAccepted);
}

void UTradeComponent::Client_SyncTrade_Implementation(const TArray<FTradeItemSlot>& MyItems, int32 MyGold,
                                                     const TArray<FTradeItemSlot>& PartnerItems, int32 PartnerGold,
                                                     bool bInMyLocked, bool bInPartnerLocked,
                                                     bool bInMyAccepted, bool bInPartnerAccepted)
{
    MyOfferedItems = MyItems;
    MyOfferedGold = MyGold;
    bMyLocked = bInMyLocked;
    bMyAccepted = bInMyAccepted;

    OnTradeItemsUpdated.Broadcast(MyItems, MyGold, PartnerItems, PartnerGold);
    OnTradeLockStateChanged.Broadcast(bInMyLocked, bInPartnerLocked, bInMyAccepted, bInPartnerAccepted);
}

void UTradeComponent::Client_TradeEnded_Implementation(bool bSuccess, const FString& Message)
{
    CurrentTradeState = bSuccess ? ETradeState::Completed : ETradeState::Cancelled;
    OnTradeEnded.Broadcast(bSuccess, Message);
    ResetTradeData();
}

void UTradeComponent::ExecuteServerTradeSwap()
{
    ANonPlayerController* PC_A = Cast<ANonPlayerController>(GetOwner());
    ANonPlayerController* PC_B = TradePartnerPC.Get();

    if (!PC_A || !PC_B)
    {
        Server_CancelTrade(TEXT("플레이어 연결이 끊겨 거래가 취소되었습니다."));
        return;
    }

    UTradeComponent* Comp_A = this;
    UTradeComponent* Comp_B = PC_B->FindComponentByClass<UTradeComponent>();

    if (!Comp_B)
    {
        Server_CancelTrade(TEXT("거래 컴포넌트 오류로 취소되었습니다."));
        return;
    }

    ANonCharacterBase* Char_A = Cast<ANonCharacterBase>(PC_A->GetPawn());
    ANonCharacterBase* Char_B = Cast<ANonCharacterBase>(PC_B->GetPawn());

    if (!Char_A || !Char_B)
    {
        Server_CancelTrade(TEXT("캐릭터를 찾을 수 없어 거래가 취소되었습니다."));
        return;
    }

    UInventoryComponent* Inven_A = Char_A->FindComponentByClass<UInventoryComponent>();
    UInventoryComponent* Inven_B = Char_B->FindComponentByClass<UInventoryComponent>();

    if (!Inven_A || !Inven_B)
    {
        Server_CancelTrade(TEXT("인벤토리 컴포넌트 오류로 취소되었습니다."));
        return;
    }

    // 1. 골드 검증
    if (Inven_A->GetGold() < Comp_A->MyOfferedGold || Inven_B->GetGold() < Comp_B->MyOfferedGold)
    {
        Server_CancelTrade(TEXT("소지 골드가 부족하여 거래가 취소되었습니다."));
        return;
    }

    // 2. 아이템 검증
    for (const FTradeItemSlot& Slot : Comp_A->MyOfferedItems)
    {
        UInventoryItem* Item = Inven_A->GetItemAt(Slot.SourceInvenSlotIndex);
        if (!Item || Item->ItemId != Slot.ItemId || Item->Quantity < Slot.Quantity)
        {
            Server_CancelTrade(TEXT("거래 등록 아이템의 정보가 변경되어 거래가 취소되었습니다."));
            return;
        }
    }
    for (const FTradeItemSlot& Slot : Comp_B->MyOfferedItems)
    {
        UInventoryItem* Item = Inven_B->GetItemAt(Slot.SourceInvenSlotIndex);
        if (!Item || Item->ItemId != Slot.ItemId || Item->Quantity < Slot.Quantity)
        {
            Server_CancelTrade(TEXT("거래 등록 아이템의 정보가 변경되어 거래가 취소되었습니다."));
            return;
        }
    }

    // 3. 아이템 교환 (원자적 처리)
    // A의 아이템 제거 -> B에게 추가
    for (const FTradeItemSlot& Slot : Comp_A->MyOfferedItems)
    {
        Inven_A->RemoveAt(Slot.SourceInvenSlotIndex, Slot.Quantity);
        int32 DummySlot = INDEX_NONE;
        Inven_B->AddItem(Slot.ItemId, Slot.Quantity, DummySlot);
    }

    // B의 아이템 제거 -> A에게 추가
    for (const FTradeItemSlot& Slot : Comp_B->MyOfferedItems)
    {
        Inven_B->RemoveAt(Slot.SourceInvenSlotIndex, Slot.Quantity);
        int32 DummySlot = INDEX_NONE;
        Inven_A->AddItem(Slot.ItemId, Slot.Quantity, DummySlot);
    }

    // 4. 골드 교환
    if (Comp_A->MyOfferedGold > 0)
    {
        Inven_A->RemoveGold(Comp_A->MyOfferedGold);
        Inven_B->AddGold(Comp_A->MyOfferedGold);
    }
    if (Comp_B->MyOfferedGold > 0)
    {
        Inven_B->RemoveGold(Comp_B->MyOfferedGold);
        Inven_A->AddGold(Comp_B->MyOfferedGold);
    }

    // 5. 성공 종료 알림
    Comp_A->Client_TradeEnded(true, TEXT("거래가 성공적으로 성사되었습니다!"));
    Comp_B->Client_TradeEnded(true, TEXT("거래가 성공적으로 성사되었습니다!"));

    Comp_A->ResetTradeData();
    Comp_B->ResetTradeData();
}
