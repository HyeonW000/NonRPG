#include "UI/Chat/ChatBoxWidget.h"
#include "UI/Chat/ChatMessageEntryWidget.h"
#include "Core/NonPlayerController.h"
#include "Character/NonCharacterBase.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UChatBoxWidget::NativeConstruct()
{
    Super::NativeConstruct();

    CurrentBackgroundOpacity = IdleBackgroundOpacity;
    if (Border_Background)
    {
        Border_Background->SetRenderOpacity(1.0f); // 자식 글자들은 언제나 100% 선명하게!
        FLinearColor CurrentColor = Border_Background->GetBrushColor();
        CurrentColor.A = CurrentBackgroundOpacity;
        Border_Background->SetBrushColor(CurrentColor);
    }

    if (EditableTextBox_Input)
    {
        EditableTextBox_Input->OnTextChanged.AddDynamic(this, &UChatBoxWidget::HandleTextChanged);
        EditableTextBox_Input->OnTextCommitted.AddDynamic(this, &UChatBoxWidget::HandleTextCommitted);
    }

    if (Button_ChannelSelect)
    {
        Button_ChannelSelect->OnClicked.AddDynamic(this, &UChatBoxWidget::ToggleChannelMenu);
    }

    if (Button_Select_General) Button_Select_General->OnClicked.AddDynamic(this, &UChatBoxWidget::OnSelectGeneral);
    if (Button_Select_Party)   Button_Select_Party->OnClicked.AddDynamic(this, &UChatBoxWidget::OnSelectParty);
    if (Button_Select_Guild)   Button_Select_Guild->OnClicked.AddDynamic(this, &UChatBoxWidget::OnSelectGuild);
    if (Button_Select_Whisper) Button_Select_Whisper->OnClicked.AddDynamic(this, &UChatBoxWidget::OnSelectWhisper);

    if (Panel_ChannelMenu)
    {
        Panel_ChannelMenu->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (Button_Tab_All)     Button_Tab_All->OnClicked.AddDynamic(this, &UChatBoxWidget::OnClickTabAll);
    if (Button_Tab_General) Button_Tab_General->OnClicked.AddDynamic(this, &UChatBoxWidget::OnClickTabGeneral);
    if (Button_Tab_Party)   Button_Tab_Party->OnClicked.AddDynamic(this, &UChatBoxWidget::OnClickTabParty);
    if (Button_Tab_Guild)   Button_Tab_Guild->OnClicked.AddDynamic(this, &UChatBoxWidget::OnClickTabGuild);
    if (Button_Tab_Whisper) Button_Tab_Whisper->OnClicked.AddDynamic(this, &UChatBoxWidget::OnClickTabWhisper);
    if (Button_Tab_System)  Button_Tab_System->OnClicked.AddDynamic(this, &UChatBoxWidget::OnClickTabSystem);

    UpdateChannelDisplay();
    UpdateTabVisuals();
}

void UChatBoxWidget::NativeDestruct()
{
    if (EditableTextBox_Input)
    {
        EditableTextBox_Input->OnTextChanged.RemoveDynamic(this, &UChatBoxWidget::HandleTextChanged);
        EditableTextBox_Input->OnTextCommitted.RemoveDynamic(this, &UChatBoxWidget::HandleTextCommitted);
    }

    Super::NativeDestruct();
}

void UChatBoxWidget::AddChatMessage(const FChatMessage& InMsg)
{
    AllMessages.Add(InMsg);

    // 최대 보관 개수 제한
    if (AllMessages.Num() > MaxMessageHistory)
    {
        AllMessages.RemoveAt(0);
    }

    // 현재 선택된 필터 탭에 맞는지 검사
    if (CurrentFilterChannel != EChatChannel::All && InMsg.Channel != CurrentFilterChannel)
    {
        return;
    }

    UPanelWidget* TargetContainer = ChatMessageContainer ? ChatMessageContainer : Cast<UPanelWidget>(ScrollBox_ChatList);

    if (TargetContainer && MessageEntryWidgetClass)
    {
        if (UChatMessageEntryWidget* Entry = CreateWidget<UChatMessageEntryWidget>(this, MessageEntryWidgetClass))
        {
            Entry->InitMessage(InMsg);
            TargetContainer->AddChild(Entry);

            // 오래된 엔트리 위젯 개수 정리
            if (TargetContainer->GetChildrenCount() > MaxMessageHistory)
            {
                TargetContainer->RemoveChildAt(0);
            }

            // 최신 메시지로 자동 스크롤!
            if (ScrollBox_ChatList)
            {
                ScrollBox_ChatList->ScrollToEnd();
            }
        }
    }
}

void UChatBoxWidget::FocusChatInput()
{
    if (EditableTextBox_Input)
    {
        EditableTextBox_Input->SetVisibility(ESlateVisibility::Visible);
        EditableTextBox_Input->SetUserFocus(GetOwningPlayer());
        EditableTextBox_Input->SetKeyboardFocus();

        if (APlayerController* PC = GetOwningPlayer())
        {
            FInputModeGameAndUI Mode;
            Mode.SetWidgetToFocus(EditableTextBox_Input->TakeWidget());
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(Mode);
            PC->bShowMouseCursor = true;
        }
    }
}

void UChatBoxWidget::UnfocusChatInput()
{
    CloseChannelMenu();

    if (EditableTextBox_Input)
    {
        EditableTextBox_Input->SetText(FText::GetEmpty());

        if (APlayerController* PC = GetOwningPlayer())
        {
            FInputModeGameOnly Mode;
            PC->SetInputMode(Mode);
            PC->bShowMouseCursor = false;
        }
    }
}

bool UChatBoxWidget::IsChatInputFocused() const
{
    if (EditableTextBox_Input)
    {
        return EditableTextBox_Input->HasKeyboardFocus() || EditableTextBox_Input->HasUserFocus(GetOwningPlayer());
    }
    return false;
}

void UChatBoxWidget::SetActiveChannel(EChatChannel NewChannel)
{
    CurrentSendChannel = NewChannel;
    UpdateChannelDisplay();
    RefocusChatInput();
}

void UChatBoxWidget::FilterByChannel(EChatChannel Channel)
{
    CurrentFilterChannel = Channel;
    UpdateTabVisuals();

    UPanelWidget* TargetContainer = ChatMessageContainer ? ChatMessageContainer : Cast<UPanelWidget>(ScrollBox_ChatList);

    if (!TargetContainer || !MessageEntryWidgetClass) return;

    TargetContainer->ClearChildren();

    for (const FChatMessage& Msg : AllMessages)
    {
        if (Channel == EChatChannel::All || Msg.Channel == Channel)
        {
            if (UChatMessageEntryWidget* Entry = CreateWidget<UChatMessageEntryWidget>(this, MessageEntryWidgetClass))
            {
                Entry->InitMessage(Msg);
                TargetContainer->AddChild(Entry);
            }
        }
    }

    if (ScrollBox_ChatList)
    {
        ScrollBox_ChatList->ScrollToEnd();
    }
}

bool UChatBoxWidget::CheckSlashCommandOnType(const FString& InText)
{
    // 공백으로 끝나지 않았으면 아직 명령어 입력 중
    if (!InText.EndsWith(TEXT(" ")))
    {
        return false;
    }

    const FString Trimmed = InText.TrimStartAndEnd();

    // 1. 단순 채널 전환 명령어 (/s, /ㄴ, /p, /ㅔ, /g, /ㅎ, /일반, /파티, /길드 등)
    if (Trimmed.Equals(TEXT("/s"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("/ㄴ"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("/일반"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("/일"), ESearchCase::IgnoreCase))
    {
        SetActiveChannel(EChatChannel::General);
        if (EditableTextBox_Input) EditableTextBox_Input->SetText(FText::GetEmpty());
        return true;
    }
    if (Trimmed.Equals(TEXT("/p"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("/ㅔ"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("/파티"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("/파"), ESearchCase::IgnoreCase))
    {
        SetActiveChannel(EChatChannel::Party);
        if (EditableTextBox_Input) EditableTextBox_Input->SetText(FText::GetEmpty());
        return true;
    }
    if (Trimmed.Equals(TEXT("/g"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("/ㅎ"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("/길드"), ESearchCase::IgnoreCase) || Trimmed.Equals(TEXT("/길"), ESearchCase::IgnoreCase))
    {
        SetActiveChannel(EChatChannel::Guild);
        if (EditableTextBox_Input) EditableTextBox_Input->SetText(FText::GetEmpty());
        return true;
    }

    // 2. 귓속말 명령어: /w 닉네임 [공백] 또는 /ㅈ 닉네임 [공백]
    if (Trimmed.StartsWith(TEXT("/w ")) || Trimmed.StartsWith(TEXT("/ㅈ ")) || Trimmed.StartsWith(TEXT("/귓 ")) || Trimmed.StartsWith(TEXT("/귓속말 ")))
    {
        TArray<FString> Tokens;
        Trimmed.ParseIntoArrayWS(Tokens);
        if (Tokens.Num() == 2)
        {
            CurrentWhisperTarget = Tokens[1];
            SetActiveChannel(EChatChannel::Whisper);
            if (EditableTextBox_Input) EditableTextBox_Input->SetText(FText::GetEmpty());
            return true;
        }
    }

    return false;
}

bool UChatBoxWidget::ParseSlashCommandOnCommit(const FString& InText, FString& OutCleanText, EChatChannel& OutChannel, FString& OutWhisperTarget)
{
    OutChannel = CurrentSendChannel;
    OutWhisperTarget = CurrentWhisperTarget;
    OutCleanText = InText;

    if (!InText.StartsWith(TEXT("/")))
    {
        return false;
    }

    TArray<FString> Tokens;
    InText.ParseIntoArrayWS(Tokens);
    if (Tokens.Num() == 0) return false;

    const FString Cmd = Tokens[0].ToLower();

    if (Cmd == TEXT("/s") || Cmd == TEXT("/ㄴ") || Cmd == TEXT("/일반") || Cmd == TEXT("/일"))
    {
        OutChannel = EChatChannel::General;
        Tokens.RemoveAt(0);
        OutCleanText = FString::Join(Tokens, TEXT(" "));
        return true;
    }
    if (Cmd == TEXT("/p") || Cmd == TEXT("/ㅔ") || Cmd == TEXT("/파티") || Cmd == TEXT("/파"))
    {
        OutChannel = EChatChannel::Party;
        Tokens.RemoveAt(0);
        OutCleanText = FString::Join(Tokens, TEXT(" "));
        return true;
    }
    if (Cmd == TEXT("/g") || Cmd == TEXT("/ㅎ") || Cmd == TEXT("/길드") || Cmd == TEXT("/길"))
    {
        OutChannel = EChatChannel::Guild;
        Tokens.RemoveAt(0);
        OutCleanText = FString::Join(Tokens, TEXT(" "));
        return true;
    }
    if (Cmd == TEXT("/w") || Cmd == TEXT("/ㅈ") || Cmd == TEXT("/귓") || Cmd == TEXT("/귓속말"))
    {
        if (Tokens.Num() >= 2)
        {
            OutChannel = EChatChannel::Whisper;
            OutWhisperTarget = Tokens[1];
            Tokens.RemoveAt(0); // 명령어 제거
            Tokens.RemoveAt(0); // 닉네임 제거
            OutCleanText = FString::Join(Tokens, TEXT(" "));
            return true;
        }
    }

    return false;
}

void UChatBoxWidget::HandleTextChanged(const FText& Text)
{
    const FString CurrentString = Text.ToString();

    // 1. 슬래시 명령어 타이핑 감지 (/s , /p , /g , /w 닉네임 )
    if (CheckSlashCommandOnType(CurrentString))
    {
        return;
    }

    // 2. 최대 글자 수 초과 방지
    if (CurrentString.Len() > MaxChatTextLength)
    {
        if (EditableTextBox_Input)
        {
            const FString Truncated = CurrentString.Left(MaxChatTextLength);
            EditableTextBox_Input->SetText(FText::FromString(Truncated));
        }
    }
}

void UChatBoxWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnEnter)
    {
        FString Trimmed = Text.ToString().TrimStartAndEnd();

        // 엔터 커밋 시 슬래시 명령어(/w 닉네임 할말 등) 파싱
        EChatChannel TargetChannel = CurrentSendChannel;
        FString WhisperTarget = CurrentWhisperTarget;
        FString ParsedText;
        if (ParseSlashCommandOnCommit(Trimmed, ParsedText, TargetChannel, WhisperTarget))
        {
            Trimmed = ParsedText;
            CurrentSendChannel = TargetChannel;
            CurrentWhisperTarget = WhisperTarget;
            UpdateChannelDisplay();
        }

        if (Trimmed.Len() > MaxChatTextLength)
        {
            Trimmed = Trimmed.Left(MaxChatTextLength);
        }

        // ── 🎁 [치트/테스트] 채팅창 아이템 지급 명령어 (/item, /아이템) ──
        if (Trimmed.Equals(TEXT("/item"), ESearchCase::IgnoreCase) || 
            Trimmed.Equals(TEXT("/아이템"), ESearchCase::IgnoreCase) ||
            Trimmed.Equals(TEXT("/치트"), ESearchCase::IgnoreCase))
        {
            if (ANonPlayerController* PC = Cast<ANonPlayerController>(GetOwningPlayer()))
            {
                const TArray<FName> TestItems = {
                    FName("Greatsword_Bronze"), FName("Sword_Iron"), FName("WoodShield"),
                    FName("Potion"), FName("Potion1"), FName("Helm"), FName("Staff")
                };
                PC->Server_CheatAddItems(TestItems, 1);
                AddLocalSystemMessage(TEXT("[시스템] 테스트 아이템 7종 지급 요청을 서버로 전송했습니다!"));
            }
            UnfocusChatInput();
            return;
        }

        if (!Trimmed.IsEmpty())
        {
            FString WarningText;
            if (!CheckAntiSpam(Trimmed, WarningText))
            {
                AddLocalSystemMessage(WarningText);

                if (bKeepFocusAfterSend)
                {
                    if (UWorld* World = GetWorld())
                    {
                        World->GetTimerManager().SetTimerForNextTick(this, &UChatBoxWidget::RefocusChatInput);
                    }
                    return;
                }
            }
            else
            {
                if (ANonPlayerController* PC = Cast<ANonPlayerController>(GetOwningPlayer()))
                {
                    FChatMessage NewMsg;
                    NewMsg.Channel = TargetChannel;
                    NewMsg.SenderName = PC->GetPlayerNickname();
                    NewMsg.Message = Trimmed;
                    NewMsg.Timestamp = FDateTime::Now();

                    PC->Server_SendChatMessage(NewMsg);
                }

                if (bKeepFocusAfterSend)
                {
                    if (EditableTextBox_Input)
                    {
                        EditableTextBox_Input->SetText(FText::GetEmpty());
                    }

                    if (UWorld* World = GetWorld())
                    {
                        World->GetTimerManager().SetTimerForNextTick(this, &UChatBoxWidget::RefocusChatInput);
                    }
                    return;
                }
            }
        }

        UnfocusChatInput();
    }
    else if (CommitMethod == ETextCommit::OnCleared || CommitMethod == ETextCommit::OnUserMovedFocus)
    {
        UnfocusChatInput();
    }
}

void UChatBoxWidget::RefocusChatInput()
{
    if (EditableTextBox_Input)
    {
        EditableTextBox_Input->SetVisibility(ESlateVisibility::Visible);
        EditableTextBox_Input->SetUserFocus(GetOwningPlayer());
        EditableTextBox_Input->SetKeyboardFocus();

        if (APlayerController* PC = GetOwningPlayer())
        {
            FInputModeGameAndUI Mode;
            Mode.SetWidgetToFocus(EditableTextBox_Input->TakeWidget());
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(Mode);
            PC->bShowMouseCursor = true;
        }
    }
}

void UChatBoxWidget::OnClickTabAll()     { FilterByChannel(EChatChannel::All); }
void UChatBoxWidget::OnClickTabGeneral() { FilterByChannel(EChatChannel::General); }
void UChatBoxWidget::OnClickTabParty()   { FilterByChannel(EChatChannel::Party); }
void UChatBoxWidget::OnClickTabGuild()   { FilterByChannel(EChatChannel::Guild); }
void UChatBoxWidget::OnClickTabWhisper() { FilterByChannel(EChatChannel::Whisper); }
void UChatBoxWidget::OnClickTabSystem()  { FilterByChannel(EChatChannel::System); }

FReply UChatBoxWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    // 입력창에서 Tab 키를 누르면 다음 발신 채널로 뿅 전환!
    if (InKeyEvent.GetKey() == EKeys::Tab)
    {
        CycleNextSendChannel();
        return FReply::Handled();
    }

    // ESC 키를 누르면 채팅창 닫고 게임 모드로 즉시 복귀!
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        UnfocusChatInput();
        return FReply::Handled();
    }

    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UChatBoxWidget::CycleNextSendChannel()
{
    // 일반 ➔ 파티 ➔ 길드 ➔ 귓속말 ➔ 일반 순환
    switch (CurrentSendChannel)
    {
    case EChatChannel::General:
        CurrentSendChannel = EChatChannel::Party;
        break;
    case EChatChannel::Party:
        CurrentSendChannel = EChatChannel::Guild;
        break;
    case EChatChannel::Guild:
        CurrentSendChannel = EChatChannel::Whisper;
        break;
    case EChatChannel::Whisper:
    default:
        CurrentSendChannel = EChatChannel::General;
        break;
    }

    UpdateChannelDisplay();
}

void UChatBoxWidget::UpdateChannelDisplay()
{
    if (!Text_CurrentChannel) return;

    FString ChannelName = TEXT("일반");
    FLinearColor ChannelColor = FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);

    switch (CurrentSendChannel)
    {
    case EChatChannel::General:
        ChannelName = TEXT("일반");
        ChannelColor = FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);
        break;
    case EChatChannel::Party:
        ChannelName = TEXT("파티");
        ChannelColor = FLinearColor(0.2f, 0.85f, 1.0f, 1.0f);
        break;
    case EChatChannel::Guild:
        ChannelName = TEXT("길드");
        ChannelColor = FLinearColor(0.3f, 1.0f, 0.4f, 1.0f);
        break;
    case EChatChannel::Whisper:
        ChannelName = CurrentWhisperTarget.IsEmpty() ? TEXT("귓속말") : FString::Printf(TEXT("귓속말(%s)"), *CurrentWhisperTarget);
        ChannelColor = FLinearColor(1.0f, 0.45f, 0.85f, 1.0f);
        break;
    default:
        break;
    }

    Text_CurrentChannel->SetText(FText::FromString(ChannelName));
    Text_CurrentChannel->SetColorAndOpacity(FSlateColor(ChannelColor));
}

void UChatBoxWidget::UpdateTabVisuals()
{
    // 1. 블루프린트 이벤트 호출 (에디터 그래프에서 애니메이션이나 커스텀 스타일링을 하고 싶을 때 활용)
    BP_OnTabChanged(CurrentFilterChannel);

    // 2. C++ 자동 색상 틴트가 켜져 있을 때만 색상 덮어쓰기 (기본값 false면 에디터 UMG 디자인 100% 보존!)
    if (bAutoTintTabButtons)
    {
        auto SetButtonVisual = [this](UButton* Btn, bool bActive)
        {
            if (!Btn) return;
            const FLinearColor TintColor = bActive ? ActiveTabColor : InactiveTabColor;
            Btn->SetBackgroundColor(TintColor);
        };

        SetButtonVisual(Button_Tab_All,     CurrentFilterChannel == EChatChannel::All);
        SetButtonVisual(Button_Tab_General, CurrentFilterChannel == EChatChannel::General);
        SetButtonVisual(Button_Tab_Party,   CurrentFilterChannel == EChatChannel::Party);
        SetButtonVisual(Button_Tab_Guild,   CurrentFilterChannel == EChatChannel::Guild);
        SetButtonVisual(Button_Tab_Whisper, CurrentFilterChannel == EChatChannel::Whisper);
        SetButtonVisual(Button_Tab_System,  CurrentFilterChannel == EChatChannel::System);
    }
}

void UChatBoxWidget::ToggleChannelMenu()
{
    if (!Panel_ChannelMenu) return;

    if (Panel_ChannelMenu->GetVisibility() == ESlateVisibility::Visible ||
        Panel_ChannelMenu->GetVisibility() == ESlateVisibility::SelfHitTestInvisible)
    {
        CloseChannelMenu();
    }
    else
    {
        OpenChannelMenu();
    }
}

void UChatBoxWidget::OpenChannelMenu()
{
    if (Panel_ChannelMenu)
    {
        Panel_ChannelMenu->SetVisibility(ESlateVisibility::Visible);
    }
}

void UChatBoxWidget::CloseChannelMenu()
{
    if (Panel_ChannelMenu)
    {
        Panel_ChannelMenu->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UChatBoxWidget::OnSelectGeneral()
{
    SetActiveChannel(EChatChannel::General);
    CloseChannelMenu();
}

void UChatBoxWidget::OnSelectParty()
{
    SetActiveChannel(EChatChannel::Party);
    CloseChannelMenu();
}

void UChatBoxWidget::OnSelectGuild()
{
    SetActiveChannel(EChatChannel::Guild);
    CloseChannelMenu();
}

void UChatBoxWidget::OnSelectWhisper()
{
    SetActiveChannel(EChatChannel::Whisper);
    CloseChannelMenu();
}

FReply UChatBoxWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        const FVector2D MouseScreenPos = InMouseEvent.GetScreenSpacePosition();

        // 1. 위아래 크기 조절 핸들 검사
        if (DragHandle_Resize && DragHandle_Resize->GetCachedGeometry().IsUnderLocation(MouseScreenPos))
        {
            if (UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(this))
            {
                bIsResizingWindow = true;
                bIsDraggingWindow = false;
                DragStartScreenPos = MouseScreenPos;
                DragStartSlotPos = CanvasSlot->GetPosition();
                DragStartSlotSize = CanvasSlot->GetSize();
                return FReply::Handled().CaptureMouse(TakeWidget());
            }
        }

        // 2. 위치 이동 핸들 검사
        if (DragHandle_Move && DragHandle_Move->GetCachedGeometry().IsUnderLocation(MouseScreenPos))
        {
            if (UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(this))
            {
                bIsDraggingWindow = true;
                bIsResizingWindow = false;
                DragStartScreenPos = MouseScreenPos;
                DragStartSlotPos = CanvasSlot->GetPosition();
                DragStartSlotSize = CanvasSlot->GetSize();
                return FReply::Handled().CaptureMouse(TakeWidget());
            }
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UChatBoxWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (bIsDraggingWindow || bIsResizingWindow)
        {
            bIsDraggingWindow = false;
            bIsResizingWindow = false;
            return FReply::Handled().ReleaseMouseCapture();
        }
    }

    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UChatBoxWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsDraggingWindow)
    {
        if (UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(this))
        {
            const FVector2D Delta = InMouseEvent.GetScreenSpacePosition() - DragStartScreenPos;
            CanvasSlot->SetPosition(DragStartSlotPos + Delta);
            return FReply::Handled();
        }
    }
    else if (bIsResizingWindow)
    {
        if (UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(this))
        {
            // 상단 핸들을 위로 끌어올리면 높이(Height) 증가
            const float DeltaY = DragStartScreenPos.Y - InMouseEvent.GetScreenSpacePosition().Y;
            const float NewHeight = FMath::Clamp(DragStartSlotSize.Y + DeltaY, MinChatHeight, MaxChatHeight);
            
            CanvasSlot->SetSize(FVector2D(DragStartSlotSize.X, NewHeight));
            return FReply::Handled();
        }
    }

    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FCursorReply UChatBoxWidget::NativeOnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent)
{
    const FVector2D MouseScreenPos = CursorEvent.GetScreenSpacePosition();

    // 1. 상단 엣지 영역에 마우스를 올리면 자동으로 상하 리사이즈 커서(↕)로 변경!
    if (DragHandle_Resize && DragHandle_Resize->GetCachedGeometry().IsUnderLocation(MouseScreenPos))
    {
        return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
    }

    // 2. 이동 핸들에 마우스를 올리면 십자 이동 커서로 변경!
    if (DragHandle_Move && DragHandle_Move->GetCachedGeometry().IsUnderLocation(MouseScreenPos))
    {
        return FCursorReply::Cursor(EMouseCursor::CardinalCross);
    }

    return Super::NativeOnCursorQuery(MyGeometry, CursorEvent);
}

void UChatBoxWidget::AddLocalSystemMessage(const FString& InSystemText)
{
    FChatMessage SysMsg;
    SysMsg.Channel = EChatChannel::System;
    SysMsg.SenderName = TEXT("시스템");
    SysMsg.Message = InSystemText;
    SysMsg.Timestamp = FDateTime::Now();

    AddChatMessage(SysMsg);
}

bool UChatBoxWidget::CheckAntiSpam(const FString& InMessage, FString& OutWarningText)
{
    const double CurrentTime = FPlatformTime::Seconds();

    // 1. 현재 도배 차단 쿨다운 중인지 검사
    if (CurrentTime < SpamCooldownEndTime)
    {
        const float Remaining = FMath::CeilToFloat(SpamCooldownEndTime - CurrentTime);
        OutWarningText = FString::Printf(TEXT("채팅을 너무 빠르게 입력하셨습니다. (%.0f초 후 다시 시도)"), Remaining);
        return false;
    }

    // 2. 동일 문장 연속 전송 검사
    if (InMessage.Equals(LastSentMessageText, ESearchCase::IgnoreCase))
    {
        DuplicateCount++;
        if (DuplicateCount > MaxDuplicateAllowed)
        {
            OutWarningText = TEXT("동일한 메시지를 연속으로 보낼 수 없습니다.");
            return false;
        }
    }
    else
    {
        LastSentMessageText = InMessage;
        DuplicateCount = 1;
    }

    // 3. 시간 윈도우 내 광속 연타 검사 (최근 N초 내 메시지 개수)
    RecentMessageTimes.RemoveAll([CurrentTime, this](double Time)
    {
        return (CurrentTime - Time) > SpamWindowSeconds;
    });

    RecentMessageTimes.Add(CurrentTime);

    if (RecentMessageTimes.Num() >= MaxMessagesInWindow)
    {
        SpamCooldownEndTime = CurrentTime + SpamCooldownDuration;
        RecentMessageTimes.Empty();
        OutWarningText = FString::Printf(TEXT("채팅을 너무 빠르게 입력하셨습니다. (%.0f초 후 다시 시도)"), SpamCooldownDuration);
        return false;
    }

    return true;
}

void UChatBoxWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    bIsMouseHovering = true;
}

void UChatBoxWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);
    bIsMouseHovering = false;
}

void UChatBoxWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // 마우스를 올렸거나 채팅 입력 중일 때는 배경이 어두워지고(Hover), 마우스가 빠지면 투명해짐(Idle)
    const float TargetOpacity = (bIsMouseHovering || IsChatInputFocused()) ? HoverBackgroundOpacity : IdleBackgroundOpacity;
    
    CurrentBackgroundOpacity = FMath::FInterpTo(CurrentBackgroundOpacity, TargetOpacity, InDeltaTime, FadeInterpSpeed);

    if (Border_Background)
    {
        FLinearColor CurrentColor = Border_Background->GetBrushColor();
        CurrentColor.A = CurrentBackgroundOpacity;
        Border_Background->SetBrushColor(CurrentColor);
    }
}
