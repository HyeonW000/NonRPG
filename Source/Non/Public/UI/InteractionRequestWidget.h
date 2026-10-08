#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionRequestWidget.generated.h"

class ANonPlayerController;
class UButton;
class UTextBlock;
class UProgressBar;
class UWidgetAnimation;

/**
 * 다른 플레이어로부터 들어온 요청의 종류
 */
UENUM(BlueprintType)
enum class EInteractionRequestType : uint8
{
    PartyInvite     UMETA(DisplayName = "파티 초대"),
    TradeRequest    UMETA(DisplayName = "1:1 거래 신청"),
    DuelRequest     UMETA(DisplayName = "1:1 결투 신청")
};

/**
 * 파티 초대, 1:1 거래, 결투 신청 등이 도착했을 때 화면 우측 하단에 스르륵 뜨는 통합 알림 팝업 위젯
 */
UCLASS(Blueprintable, BlueprintType)
class NON_API UInteractionRequestWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UInteractionRequestWidget(const FObjectInitializer& ObjectInitializer);

    /** 요청 알림 초기화 및 시작 */
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void SetupRequest(EInteractionRequestType InType, const FString& InRequesterName, ANonPlayerController* InRequesterPC, float InTimeoutSeconds = 15.f);

    /** 수락 */
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void AcceptRequest();

    /** 거절 */
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void DeclineRequest();

    /** 팝업 닫기 */
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void ClosePopup();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    // ── UMG 바인딩 위젯 (Blueprint WBP에서 이름만 일치시키면 자동 연결) ──
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI")
    TObjectPtr<UTextBlock> Text_Title;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI")
    TObjectPtr<UTextBlock> Text_RequesterName;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI")
    TObjectPtr<UTextBlock> Text_Message;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI")
    TObjectPtr<UTextBlock> Text_Timer;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI")
    TObjectPtr<UProgressBar> ProgressBar_Timer;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI")
    TObjectPtr<UButton> Button_Accept;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI")
    TObjectPtr<UButton> Button_Decline;

    // ── 애니메이션 자동 바인딩 (WBP에서 SlideIn / SlideOut 이름으로 애니메이션을 만들면 C++이 자동 재생) ──
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional), BlueprintReadOnly, Category = "UI|Animation")
    TObjectPtr<UWidgetAnimation> SlideIn;

    UPROPERTY(Transient, meta = (BindWidgetAnimOptional), BlueprintReadOnly, Category = "UI|Animation")
    TObjectPtr<UWidgetAnimation> SlideOut;

    UWidgetAnimation* FindWidgetAnimation(const FString& AnimName) const;

    // ── 블루프린트 애니메이션 훅 (우측 하단 슬라이드 인/아웃 등) ──
    UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
    void BP_OnRequestOpened(EInteractionRequestType InRequestType, const FString& InRequesterName);

    UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
    void BP_OnRequestClosed();

    UFUNCTION()
    void OnSlideOutFinished();

private:
    UPROPERTY()
    EInteractionRequestType RequestType = EInteractionRequestType::PartyInvite;

    UPROPERTY()
    FString RequesterName;

    UPROPERTY()
    TWeakObjectPtr<ANonPlayerController> RequesterPC;

    float RemainingTime = 15.f;
    float TotalTimeout = 15.f;
    bool bHasResponded = false;
};
