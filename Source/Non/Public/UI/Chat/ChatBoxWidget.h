#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Chat/ChatTypes.h"
#include "Types/SlateEnums.h"
#include "ChatBoxWidget.generated.h"

class UScrollBox;
class UEditableTextBox;
class UButton;
class UChatMessageEntryWidget;

/**
 * 🌟 인게임 메인 채팅창 위젯
 */
UCLASS()
class NON_API UChatBoxWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** 새 채팅 메시지 수신 및 리스트에 추가 */
    UFUNCTION(BlueprintCallable, Category = "Chat")
    void AddChatMessage(const FChatMessage& InMsg);

    /** 채팅 입력창 포커스 활성화 (타이핑 모드) */
    UFUNCTION(BlueprintCallable, Category = "Chat")
    void FocusChatInput();

    /** 채팅 입력창 포커스 해제 (게임 모드 복귀) */
    UFUNCTION(BlueprintCallable, Category = "Chat")
    void UnfocusChatInput();

    /** 현재 채팅 입력창에 포커스가 있는지 */
    UFUNCTION(BlueprintPure, Category = "Chat")
    bool IsChatInputFocused() const;

    /** 현재 활성화된 채팅 채널 변경 */
    UFUNCTION(BlueprintCallable, Category = "Chat")
    void SetActiveChannel(EChatChannel NewChannel);

    /** 특정 채널 필터링 (All이면 전부 노출) */
    UFUNCTION(BlueprintCallable, Category = "Chat")
    void FilterByChannel(EChatChannel Channel);

    /** 다음 발신 채널로 순환 전환 (일반 ➔ 파티 ➔ 길드 ➔ 귓속말 ➔ 일반) */
    UFUNCTION(BlueprintCallable, Category = "Chat")
    void CycleNextSendChannel();

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FCursorReply NativeOnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) override;
    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

    /* ===== UMG 바인딩 위젯 ===== */

    /** 채팅창 메인 배경 Border (마우스 호버 시 어두워지고 빠지면 투명해짐) */
    UPROPERTY(meta = (BindWidgetOptional))
    class UBorder* Border_Background = nullptr;

    /** 마우스가 없을 때 평상시 배경 불투명도 (0.0 = 완전 투명, 0.15 = 은은한 투명) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|Fade")
    float IdleBackgroundOpacity = 0.15f;

    /** 마우스를 올렸거나 입력 중일 때 배경 불투명도 (0.75 = 어둡고 선명한 반투명) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|Fade")
    float HoverBackgroundOpacity = 0.75f;

    /** 투명도 부드러운 전환 속도 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|Fade")
    float FadeInterpSpeed = 8.0f;

    /** 드래그 위치 이동 핸들 (상단 십자 아이콘 또는 상단 바: Border/Button/Image) */
    UPROPERTY(meta = (BindWidgetOptional))
    UWidget* DragHandle_Move = nullptr;

    /** 위아래 크기 조절 핸들 (상단 모서리 또는 상단 핸들: Border/Button/Image) */
    UPROPERTY(meta = (BindWidgetOptional))
    UWidget* DragHandle_Resize = nullptr;

    /** 채팅창 최소/최대 높이 조절 범위 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|Window")
    float MinChatHeight = 160.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|Window")
    float MaxChatHeight = 700.0f;

    /** 메시지들이 쌓이는 스크롤 박스 */
    UPROPERTY(meta = (BindWidgetOptional))
    UScrollBox* ScrollBox_ChatList = nullptr;

    /** 하단 정렬을 위한 내부 메시지 컨테이너 (ScrollBox 내부의 Vertical Box 등) */
    UPROPERTY(meta = (BindWidgetOptional))
    class UPanelWidget* ChatMessageContainer = nullptr;

    /** 텍스트 입력창 */
    UPROPERTY(meta = (BindWidgetOptional))
    UEditableTextBox* EditableTextBox_Input = nullptr;

    /** 입력창 좌측 채널 선택 버튼 (클릭 시 채널 목록 팝업 열기/닫기) */
    UPROPERTY(meta = (BindWidgetOptional))
    UButton* Button_ChannelSelect = nullptr;

    /** 입력창 좌측 현재 발신 채널 텍스트 (예: 일반, 파티, 길드) */
    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* Text_CurrentChannel = nullptr;

    /** 채널 목록 드롭다운 팝업 패널 */
    UPROPERTY(meta = (BindWidgetOptional))
    class UPanelWidget* Panel_ChannelMenu = nullptr;

    UPROPERTY(meta = (BindWidgetOptional)) UButton* Button_Select_General = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* Button_Select_Party = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* Button_Select_Guild = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* Button_Select_Whisper = nullptr;

    /** 채널 목록 팝업 열기/닫기 토글 */
    UFUNCTION(BlueprintCallable, Category = "Chat")
    void ToggleChannelMenu();

    UFUNCTION(BlueprintCallable, Category = "Chat")
    void OpenChannelMenu();

    UFUNCTION(BlueprintCallable, Category = "Chat")
    void CloseChannelMenu();

    /** 단일 메시지 한 줄 위젯 클래스 */
    UPROPERTY(EditDefaultsOnly, Category = "Chat")
    TSubclassOf<UChatMessageEntryWidget> MessageEntryWidgetClass;

    /** 최대 보관 메시지 개수 (초과 시 오래된 메시지 삭제) */
    UPROPERTY(EditDefaultsOnly, Category = "Chat")
    int32 MaxMessageHistory = 100;

    /** 1회 입력 가능한 최대 글자 수 제한 (초과 시 자동 잘림) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat")
    int32 MaxChatTextLength = 80;

    // ── 🛡️ [Anti-Spam] 도배 방지 설정 ──
    /** 도배 판정 시간 윈도우 (초) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|AntiSpam")
    float SpamWindowSeconds = 1.5f;

    /** 윈도우 시간 내 최대 허용 메시지 수 (초과 시 도배로 판단) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|AntiSpam")
    int32 MaxMessagesInWindow = 3;

    /** 도배 감지 시 채팅 차단 쿨다운 시간 (초) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|AntiSpam")
    float SpamCooldownDuration = 2.0f;

    /** 동일 문장 연속 전송 허용 횟수 (3회째부터 차단) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|AntiSpam")
    int32 MaxDuplicateAllowed = 2;

    /** 엔터로 메시지 전송 후에도 입력창 포커스를 유지할지 여부 (빈 엔터나 ESC로 닫힘) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat")
    bool bKeepFocusAfterSend = true;

    /** C++에서 탭 버튼 배경 색상을 자동으로 덮어씌울지 여부 (false면 에디터 UMG 버튼 디자인 100% 유지) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|TabStyle")
    bool bAutoTintTabButtons = false;

    /** 선택된 활성 탭 버튼 색상 (bAutoTintTabButtons가 true일 때만 적용) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|TabStyle", meta = (EditCondition = "bAutoTintTabButtons"))
    FLinearColor ActiveTabColor = FLinearColor(0.2f, 0.6f, 1.0f, 1.0f);

    /** 비선택 탭 버튼 색상 (bAutoTintTabButtons가 true일 때만 적용) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Chat|TabStyle", meta = (EditCondition = "bAutoTintTabButtons"))
    FLinearColor InactiveTabColor = FLinearColor(0.4f, 0.4f, 0.4f, 0.6f);

    /** 블루프린트에서 탭 전환 시 시각 효과(애니메이션/스타일)를 자유롭게 커스텀할 수 있는 이벤트 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Chat")
    void BP_OnTabChanged(EChatChannel CurrentTab);

    /** 채널 탭 버튼들 (선택 사항) */
    UPROPERTY(meta = (BindWidgetOptional))
    UButton* Button_Tab_All = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* Button_Tab_General = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* Button_Tab_Party = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* Button_Tab_Guild = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* Button_Tab_Whisper = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* Button_Tab_System = nullptr;

    /** 탭 버튼들의 하이라이트 불 켜기/끄기 갱신 */
    UFUNCTION(BlueprintCallable, Category = "Chat")
    void UpdateTabVisuals();

private:
    /** 텍스트 입력창 타이핑 시 글자 수 실시간 제한 콜백 */
    UFUNCTION()
    void HandleTextChanged(const FText& Text);

    /** 텍스트 입력창에서 Enter 키 커밋 시 호출되는 콜백 */
    UFUNCTION()
    void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

    /** 탭 버튼 클릭 이벤트 핸들러 */
    UFUNCTION() void OnClickTabAll();
    UFUNCTION() void OnClickTabGeneral();
    UFUNCTION() void OnClickTabParty();
    UFUNCTION() void OnClickTabGuild();
    UFUNCTION() void OnClickTabWhisper();
    UFUNCTION() void OnClickTabSystem();

    /** 채널 목록 팝업 메뉴 내 버튼 클릭 핸들러 */
    UFUNCTION() void OnSelectGeneral();
    UFUNCTION() void OnSelectParty();
    UFUNCTION() void OnSelectGuild();
    UFUNCTION() void OnSelectWhisper();

    /** 슬레이트 커밋 후 다음 틱에 입력창 포커스 재고정 */
    void RefocusChatInput();

    /** 전체 메시지 히스토리 */
    TArray<FChatMessage> AllMessages;

    /** 현재 보고 있는 필터 채널 */
    EChatChannel CurrentFilterChannel = EChatChannel::All;

    /** 현재 내가 작성 중인 발신 채널 */
    EChatChannel CurrentSendChannel = EChatChannel::General;

    /** 현재 지정된 귓속말 상대 플레이어 닉네임 */
    FString CurrentWhisperTarget;

    /** 타이핑 중 슬래시 명령어(/s, /p, /g, /w 닉네임) 감지 및 자동 채널 전환 (true면 입력창 비움) */
    bool CheckSlashCommandOnType(const FString& InText);

    /** 엔터 전송 시 슬래시 명령어 파싱 */
    bool ParseSlashCommandOnCommit(const FString& InText, FString& OutCleanText, EChatChannel& OutChannel, FString& OutWhisperTarget);

    /** 슬래시 명령어 실행 (/파티초대, /탈퇴, /길드초대, /도움말, /item 등) */
    void HandleSlashCommand(const FString& InCommandStr);

    /** 하단 입력창 좌측 채널 텍스트/색상 갱신 */
    void UpdateChannelDisplay();

    /** 로컬 시스템 알림 메시지 추가 (경고/안내용) */
    void AddLocalSystemMessage(const FString& InSystemText);

    /** 도배 및 중복 메시지 검사 (true면 전송 허용, false면 차단) */
    bool CheckAntiSpam(const FString& InMessage, FString& OutWarningText);

    /** 윈도우 드래그 이동 및 리사이즈 상태 */
    bool bIsDraggingWindow = false;
    bool bIsResizingWindow = false;
    FVector2D DragStartScreenPos = FVector2D::ZeroVector;
    FVector2D DragStartSlotPos = FVector2D::ZeroVector;
    FVector2D DragStartSlotSize = FVector2D::ZeroVector;

    /** 도배 방지 런타임 상태 */
    TArray<double> RecentMessageTimes;
    double SpamCooldownEndTime = 0.0;
    FString LastSentMessageText;
    int32 DuplicateCount = 0;

    /** 배경 투명도 자동 조절 런타임 상태 */
    bool bIsMouseHovering = false;
    float CurrentBackgroundOpacity = 0.15f;
};
