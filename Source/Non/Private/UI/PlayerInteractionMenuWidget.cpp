#include "UI/PlayerInteractionMenuWidget.h"
#include "Core/NonPlayerController.h"
#include "Character/NonCharacterBase.h"
#include "System/PartyComponent.h"
#include "System/TradeComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

UPlayerInteractionMenuWidget::UPlayerInteractionMenuWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UPlayerInteractionMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Button_PartyInvite)
    {
        Button_PartyInvite->OnClicked.AddDynamic(this, &UPlayerInteractionMenuWidget::OnPartyInviteClicked);
    }
    if (Button_Trade)
    {
        Button_Trade->OnClicked.AddDynamic(this, &UPlayerInteractionMenuWidget::OnTradeClicked);
    }
    if (Button_Duel)
    {
        Button_Duel->OnClicked.AddDynamic(this, &UPlayerInteractionMenuWidget::OnDuelClicked);
    }
    if (Button_Inspect)
    {
        Button_Inspect->OnClicked.AddDynamic(this, &UPlayerInteractionMenuWidget::OnInspectClicked);
    }
}

void UPlayerInteractionMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // 대상 플레이어와 거리가 1.8미터(180cm) 이상 벌어지면 메뉴 자동 닫기
    if (TargetCharacter.IsValid())
    {
        if (APawn* MyPawn = GetOwningPlayerPawn())
        {
            const float Dist2D = FVector::Dist2D(MyPawn->GetActorLocation(), TargetCharacter->GetActorLocation());
            constexpr float MaxDistance = 180.f;
            if (Dist2D > MaxDistance)
            {
                CloseMenu();
                return;
            }
        }

        // 대상 캐릭터의 3D 위치를 2D 화면 좌표로 변환하여 메뉴 위치를 캐릭터 옆에 고정
        UpdateScreenPosition();
    }
    else
    {
        CloseMenu();
    }
}

void UPlayerInteractionMenuWidget::NativeDestruct()
{
    if (Button_PartyInvite) Button_PartyInvite->OnClicked.RemoveAll(this);
    if (Button_Trade) Button_Trade->OnClicked.RemoveAll(this);
    if (Button_Duel) Button_Duel->OnClicked.RemoveAll(this);
    if (Button_Inspect) Button_Inspect->OnClicked.RemoveAll(this);

    Super::NativeDestruct();
}

void UPlayerInteractionMenuWidget::InitializeMenu(ANonCharacterBase* InTargetChar)
{
    TargetCharacter = InTargetChar;
    if (!InTargetChar) return;

    TargetPC = Cast<ANonPlayerController>(InTargetChar->GetController());

    FString TargetName = InTargetChar->GetPlayerName();
    if (TargetName.IsEmpty())
    {
        TargetName = TEXT("플레이어");
    }

    if (Text_TargetName)
    {
        Text_TargetName->SetText(FText::FromString(TargetName));
    }

    // 메뉴가 열린 첫 프레임부터 즉시 대상 캐릭터 옆에 위치하도록 갱신
    UpdateScreenPosition();

    BP_OnMenuOpened(InTargetChar, TargetName);
}

FReply UPlayerInteractionMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    // ESC 키 또는 F 키 입력 시 메뉴 닫기
    if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::F)
    {
        CloseMenu();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPlayerInteractionMenuWidget::CloseMenu()
{
    SetVisibility(ESlateVisibility::Collapsed);
    RemoveFromParent();

    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->bShowMouseCursor = false;
        FInputModeGameOnly Mode;
        PC->SetInputMode(Mode);
    }
}

void UPlayerInteractionMenuWidget::OnPartyInviteClicked()
{
    if (ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwningPlayer()))
    {
        if (TargetCharacter.IsValid())
        {
            const FString TargetName = TargetCharacter->GetPlayerName().TrimStartAndEnd();

            // 닉네임이 있으면 닉네임 기반 단일 호출, 없으면 액터 포인터 기반으로 호출하여 중복 전송 방지
            if (!TargetName.IsEmpty())
            {
                MyPC->Server_SendPartyInviteByNickname(TargetName);
            }
            else
            {
                MyPC->Server_SendPartyInviteDirect(TargetCharacter.Get());
            }

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Cyan, FString::Printf(TEXT("[%s] 님에게 파티 초대를 보냈습니다."), *TargetName));
            }
        }
        else
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("[클라 경고] TargetCharacter가 유효하지 않습니다!"));
            }
        }
    }
    else
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("[클라 경고] OwningPlayer가 NonPlayerController가 아닙니다!"));
        }
    }
    CloseMenu();
}

void UPlayerInteractionMenuWidget::OnTradeClicked()
{
    if (ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwningPlayer()))
    {
        if (TargetCharacter.IsValid())
        {
            const FString TargetName = TargetCharacter->GetPlayerName().TrimStartAndEnd();
            MyPC->Server_SendTradeRequestDirect(TargetCharacter.Get());

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Cyan, FString::Printf(TEXT("[%s] 님에게 1:1 개인 거래를 신청했습니다."), *TargetName));
            }
        }
    }
    CloseMenu();
}

void UPlayerInteractionMenuWidget::OnDuelClicked()
{
    if (ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwningPlayer()))
    {
        if (TargetCharacter.IsValid())
        {
            const FString TargetName = TargetCharacter->GetPlayerName().TrimStartAndEnd();
            MyPC->Server_RequestDuelByCharacter(TargetCharacter.Get());

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Cyan, FString::Printf(TEXT("[%s] 님에게 1:1 결투를 신청했습니다!"), *TargetName));
            }
        }
    }
    CloseMenu();
}

void UPlayerInteractionMenuWidget::OnInspectClicked()
{
    if (TargetCharacter.IsValid())
    {
        BP_OnInspectPlayer(TargetCharacter.Get());
    }
    CloseMenu();
}

void UPlayerInteractionMenuWidget::UpdateScreenPosition()
{
    if (!TargetCharacter.IsValid()) return;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    // 대상 캐릭터의 머리 높이 기준 (Z + 70cm)
    const FVector WorldLoc = TargetCharacter->GetActorLocation() + FVector(0.f, 0.f, 70.f);

    // 대상 캐릭터의 3D 위치를 플레이어 뷰포트 픽셀 좌표(bPlayerViewportRelative = true)로 투영
    FVector2D ScreenPixelPos;
    if (PC->ProjectWorldLocationToScreen(WorldLoc, ScreenPixelPos, true))
    {
        const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(PC);
        const float SafeScale = (ViewportScale > 0.0f) ? ViewportScale : 1.0f;

        // UMG 가상 해상도 좌표 (DPI 배율 반영)
        const FVector2D WidgetPos = ScreenPixelPos / SafeScale;
        const FVector2D FinalPos = WidgetPos + FVector2D(35.f, -30.f);

        // 1. 만약 CanvasPanel이 아직 존재하는 구조라면 CanvasPanelSlot을 찾아 좌상단(0, 0) 앵커로 배치
        UCanvasPanelSlot* TargetCanvasSlot = nullptr;
        for (UWidget* Curr = MenuContainer.Get(); Curr != nullptr; Curr = Curr->GetParent())
        {
            if (UCanvasPanelSlot* FoundSlot = Cast<UCanvasPanelSlot>(Curr->Slot))
            {
                TargetCanvasSlot = FoundSlot;
                break;
            }
        }

        if (TargetCanvasSlot)
        {
            TargetCanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
            TargetCanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
            TargetCanvasSlot->SetPosition(FinalPos);
            return;
        }

        // 2. [정석 구조] CanvasPanel을 제거하여 루트가 Border/SizeBox인 경우:
        // 위젯 자체를 뷰포트 상의 캐릭터 위치에 직접 배치 (DPI 왜곡 0%, 100% 칼고정!)
        SetPositionInViewport(FinalPos, false);
    }
}
