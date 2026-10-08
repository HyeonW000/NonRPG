#include "UI/InteractionRequestWidget.h"
#include "Core/NonPlayerController.h"
#include "System/PartyComponent.h"
#include "System/TradeComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"

UInteractionRequestWidget::UInteractionRequestWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UInteractionRequestWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Button_Accept)
    {
        Button_Accept->OnClicked.AddDynamic(this, &UInteractionRequestWidget::AcceptRequest);
    }
    if (Button_Decline)
    {
        Button_Decline->OnClicked.AddDynamic(this, &UInteractionRequestWidget::DeclineRequest);
    }
}

void UInteractionRequestWidget::NativeDestruct()
{
    if (Button_Accept) Button_Accept->OnClicked.RemoveAll(this);
    if (Button_Decline) Button_Decline->OnClicked.RemoveAll(this);

    Super::NativeDestruct();
}

void UInteractionRequestWidget::SetupRequest(EInteractionRequestType InType, const FString& InRequesterName, ANonPlayerController* InRequesterPC, float InTimeoutSeconds)
{
    RequestType = InType;
    RequesterName = InRequesterName;
    RequesterPC = InRequesterPC;
    TotalTimeout = FMath::Max(5.f, InTimeoutSeconds);
    RemainingTime = TotalTimeout;
    bHasResponded = false;

    FString TitleStr = TEXT("상호작용 요청");
    FString MessageStr = FString::Printf(TEXT("%s 님으로부터 요청이 도착했습니다."), *InRequesterName);

    switch (InType)
    {
    case EInteractionRequestType::PartyInvite:
        TitleStr = TEXT("파티 초대");
        MessageStr = FString::Printf(TEXT("%s 님이 파티에 초대했습니다."), *InRequesterName);
        break;
    case EInteractionRequestType::TradeRequest:
        TitleStr = TEXT("1:1 거래 신청");
        MessageStr = FString::Printf(TEXT("%s 님이 1:1 개인 거래를 신청했습니다."), *InRequesterName);
        break;
    case EInteractionRequestType::DuelRequest:
        TitleStr = TEXT("결투 신청");
        MessageStr = FString::Printf(TEXT("%s 님이 1:1 결투를 신청했습니다!"), *InRequesterName);
        break;
    }

    if (Text_Title)
    {
        Text_Title->SetText(FText::FromString(TitleStr));
    }
    if (Text_RequesterName)
    {
        Text_RequesterName->SetVisibility(ESlateVisibility::Collapsed);
        Text_RequesterName->SetText(FText::GetEmpty());
    }
    if (Text_Message)
    {
        Text_Message->SetAutoWrapText(false);
        Text_Message->SetText(FText::FromString(MessageStr));
    }

    if (Text_Timer)
    {
        Text_Timer->SetText(FText::FromString(FString::Printf(TEXT("%d초"), FMath::CeilToInt(RemainingTime))));
    }
    if (ProgressBar_Timer)
    {
        ProgressBar_Timer->SetPercent(1.0f);
    }

    BP_OnRequestOpened(InType, InRequesterName);

    // C++에서 SlideIn 애니메이션 자동 감지 및 재생
    UWidgetAnimation* AnimToPlay = SlideIn ? SlideIn.Get() : FindWidgetAnimation(TEXT("SlideIn"));
    if (AnimToPlay)
    {
        PlayAnimation(AnimToPlay);
    }
}

void UInteractionRequestWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bHasResponded) return;

    RemainingTime -= InDeltaTime;

    if (ProgressBar_Timer && TotalTimeout > 0.f)
    {
        ProgressBar_Timer->SetPercent(FMath::Clamp(RemainingTime / TotalTimeout, 0.0f, 1.0f));
    }

    if (Text_Timer)
    {
        const int32 Seconds = FMath::Max(0, FMath::CeilToInt(RemainingTime));
        Text_Timer->SetText(FText::FromString(FString::Printf(TEXT("%d초"), Seconds)));
    }

    // 시간 초과 시 자동 거절 및 창 닫기
    if (RemainingTime <= 0.f)
    {
        DeclineRequest();
    }
}

FReply UInteractionRequestWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    // Y 키 입력 시 즉시 수락
    if (InKeyEvent.GetKey() == EKeys::Y)
    {
        AcceptRequest();
        return FReply::Handled();
    }
    // N 키 또는 ESC 키 입력 시 거절
    if (InKeyEvent.GetKey() == EKeys::N || InKeyEvent.GetKey() == EKeys::Escape)
    {
        DeclineRequest();
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UInteractionRequestWidget::AcceptRequest()
{
    if (bHasResponded) return;
    bHasResponded = true;

    if (ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwningPlayer()))
    {
        switch (RequestType)
        {
        case EInteractionRequestType::PartyInvite:
            if (RequesterPC.IsValid())
            {
                MyPC->PendingPartyInviterPC = RequesterPC.Get();
                if (UPartyComponent* PartyComp = MyPC->GetPartyComponent())
                {
                    PartyComp->SetPendingInviterPC(RequesterPC.Get());
                }
            }
            if (UPartyComponent* PartyComp = MyPC->GetPartyComponent())
            {
                PartyComp->RespondToPartyInvite(true);
            }
            else
            {
                MyPC->Server_RespondPartyInviteDirect(true);
            }
            break;
        case EInteractionRequestType::TradeRequest:
            MyPC->Server_RespondTradeRequestDirect(true);
            if (UTradeComponent* TradeComp = MyPC->GetTradeComponent())
            {
                TradeComp->RespondTradeRequest(true);
            }
            break;
        case EInteractionRequestType::DuelRequest:
            if (RequesterPC.IsValid())
            {
                MyPC->Server_AcceptDuel(RequesterPC.Get());
            }
            break;
        }
    }

    ClosePopup();
}

void UInteractionRequestWidget::DeclineRequest()
{
    if (bHasResponded) return;
    bHasResponded = true;

    if (ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwningPlayer()))
    {
        switch (RequestType)
        {
        case EInteractionRequestType::PartyInvite:
            if (RequesterPC.IsValid())
            {
                MyPC->PendingPartyInviterPC = RequesterPC.Get();
                if (UPartyComponent* PartyComp = MyPC->GetPartyComponent())
                {
                    PartyComp->SetPendingInviterPC(RequesterPC.Get());
                }
            }
            if (UPartyComponent* PartyComp = MyPC->GetPartyComponent())
            {
                PartyComp->RespondToPartyInvite(false);
            }
            else
            {
                MyPC->Server_RespondPartyInviteDirect(false);
            }
            break;
        case EInteractionRequestType::TradeRequest:
            MyPC->Server_RespondTradeRequestDirect(false);
            if (UTradeComponent* TradeComp = MyPC->GetTradeComponent())
            {
                TradeComp->RespondTradeRequest(false);
            }
            break;
        case EInteractionRequestType::DuelRequest:
            if (RequesterPC.IsValid())
            {
                MyPC->Server_DeclineDuel(RequesterPC.Get());
            }
            break;
        }
    }

    ClosePopup();
}

void UInteractionRequestWidget::ClosePopup()
{
    BP_OnRequestClosed();

    // SlideOut 애니메이션이 있으면 재생 후 끝나면 닫히도록 바인딩
    UWidgetAnimation* OutAnim = SlideOut ? SlideOut.Get() : FindWidgetAnimation(TEXT("SlideOut"));
    if (OutAnim)
    {
        FWidgetAnimationDynamicEvent EndDelegate;
        EndDelegate.BindDynamic(this, &UInteractionRequestWidget::OnSlideOutFinished);
        BindToAnimationFinished(OutAnim, EndDelegate);
        PlayAnimation(OutAnim);
    }
    else
    {
        OnSlideOutFinished();
    }
}

void UInteractionRequestWidget::OnSlideOutFinished()
{
    SetVisibility(ESlateVisibility::Collapsed);
    RemoveFromParent();
}

UWidgetAnimation* UInteractionRequestWidget::FindWidgetAnimation(const FString& AnimName) const
{
    if (UWidgetBlueprintGeneratedClass* WidgetAnimClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass()))
    {
        for (UWidgetAnimation* Anim : WidgetAnimClass->Animations)
        {
            if (Anim && Anim->GetName().Contains(AnimName))
            {
                return Anim;
            }
        }
    }
    return nullptr;
}
