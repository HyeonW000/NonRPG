#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UI/Chat/ChatTypes.h"
#include "UI/InteractionRequestWidget.h"
#include "NonPlayerController.generated.h"

class UNonUIManagerComponent;
class ANonCharacterBase;
class UQuickSlotManager;
class UPartyComponent;
class UTradeComponent;

UCLASS()
class NON_API ANonPlayerController : public APlayerController {
  GENERATED_BODY()

public:
  ANonPlayerController();

  // ── 🔥 [Chat System] 채팅 시스템 RPC 및 닉네임 ──
  UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Chat")
  void Server_SendChatMessage(const FChatMessage& Message);

  UFUNCTION(Client, Reliable, BlueprintCallable, Category = "Chat")
  void Client_ReceiveChatMessage(const FChatMessage& Message);

  UFUNCTION(Client, Reliable, BlueprintCallable, Category = "Chat")
  void Client_AddSystemMessage(const FString& MessageText);

  UFUNCTION(BlueprintPure, Category = "Chat")
  FString GetPlayerNickname() const;

  UFUNCTION(BlueprintCallable, Category = "Chat")
  void SetPlayerNickname(const FString& NewNickname);

  // [Cheat / Debug] 클라이언트에서 아이템 일괄 지급 요청 Server RPC
  UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Cheat")
  void Server_CheatAddItems(const TArray<FName>& ItemIds, int32 QuantityPerItem = 1);

  /** 스폰 시 이전 세이브 데이터(위치, 인벤토리, 장비) 자동 로드 활성화 여부 (테스트 시 false 권장) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveSystem")
  bool bEnableAutoLoadOnSpawn = false;

protected:
  virtual void BeginPlay() override;
  virtual void SetupInputComponent() override;
  virtual void OnPossess(APawn *InPawn) override;
  virtual void SetPawn(APawn *InPawn) override;
  virtual void PlayerTick(float DeltaTime) override;

  static TSharedPtr<class SViewport> GetGameViewportSViewport(UWorld *World);

  // IMC만 꽂으면 자동 바인딩
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
  class UInputMappingContext *IMC_Default;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
  class UInputMappingContext *IMC_QuickSlots;

  // --- [Refactor] 명시적 Input Action 바인딩 ---
  // 캐릭터 이동/전투 관련
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_Move;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_Look;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_Jump;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_Attack;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_Dodge;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_Guard;

  // UI 관련
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_Inventory;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_SkillWindow;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_CharacterWindow;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_Interact;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_ESC;

  // ToggleArmed (Character에도 있지만 UI/Controller 레벨에서 처리할 경우)
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_ToggleArmed;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_CursorToggle = nullptr;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_Zoom = nullptr;

  /** 채팅창 입력 열기 (Enter 키) */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_Chat = nullptr;

  /** 자동 달리기 토글 (R 키) */
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
  class UInputAction *IA_AutoRun = nullptr;

  // 캐시
  UPROPERTY() ANonCharacterBase *CachedChar = nullptr;
  UPROPERTY() UQuickSlotManager *CachedQuick = nullptr;

  // ── 🏃 [Auto-Run] 자동 달리기 ──
  UFUNCTION(BlueprintCallable, Category = "Movement|AutoRun")
  void ToggleAutoRun();

  UFUNCTION(BlueprintCallable, Category = "Movement|AutoRun")
  void SetAutoRunning(bool bEnable);

  UFUNCTION(BlueprintPure, Category = "Movement|AutoRun")
  bool IsAutoRunning() const { return bIsAutoRunning; }

protected:
  void ProcessAutoRun();

  /** 현재 자동 달리기 활성화 여부 */
  UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement|AutoRun")
  bool bIsAutoRunning = false;

  // 입력 핸들러
  void OnMove(const FInputActionValue &Value);
  void OnLook(const FInputActionValue &Value);
  void OnZoom(const FInputActionValue &Value);
  void OnJumpStart(const FInputActionValue &Value);
  void OnJumpStop(const FInputActionValue &Value);
  void OnAttack(const FInputActionValue &Value);
  void OnInventory();
  void OnToggleSkillWindow();
  void OnToggleCharacterWindow();
  void OnToggleArmed(const FInputActionValue &Value);
  void OnGuardPressed(const FInputActionInstance &Instance);
  void OnGuardReleased(const FInputActionInstance &Instance);
  void OnDodge(const FInputActionValue &Value);
  void OnInteract(const FInputActionInstance &Instance);
  void OnEsc(const FInputActionInstance &Instance);
  void OnToggleChat();

private:
  UPROPERTY(Replicated)
  FString PlayerNickname;
  bool bCursorFree = false;
  void ToggleCursorLook();

  // 퀵슬롯 공통 처리
  UFUNCTION()
  void HandleQuickSlot(int32 OneBased);

  // 퀵슬롯 래퍼들 (0 키 = 10번째)
  UFUNCTION() void OnQS0(const FInputActionInstance &Instance);
  UFUNCTION() void OnQS1(const FInputActionInstance &Instance);
  UFUNCTION() void OnQS2(const FInputActionInstance &Instance);
  UFUNCTION() void OnQS3(const FInputActionInstance &Instance);
  UFUNCTION() void OnQS4(const FInputActionInstance &Instance);
  UFUNCTION() void OnQS5(const FInputActionInstance &Instance);
  UFUNCTION() void OnQS6(const FInputActionInstance &Instance);
  UFUNCTION() void OnQS7(const FInputActionInstance &Instance);
  UFUNCTION() void OnQS8(const FInputActionInstance &Instance);
  UFUNCTION() void OnQS9(const FInputActionInstance &Instance);

  // Move 입력 액션을 캐시해 두면 Dodge 시점에 현재 Move 값을 바로 읽을 수 있음
  UPROPERTY()
  TObjectPtr<const UInputAction> IA_MoveCached = nullptr;

  // 현재 바라보고 있는 상호작용 타겟
  TWeakObjectPtr<AActor> CurrentInteractTarget;

  void UpdateInteractFocus(float DeltaTime);

  // [New] 대화 종료 후 상호작용 프롬프트가 즉시 뜨는 것을 방지하기 위한 쿨다운
  float DialogueEndCooldown = 0.f;

public:
  // 대화 종료 시 호출하여 상호작용 프롬프트를 잠시 숨깁니다.
  UFUNCTION(BlueprintCallable, Category = "Interaction")
  void StartDialogueCooldown(float Duration = 1.5f);

  // [New] 타이틀 위젯 클래스
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
  TSubclassOf<class UUserWidget> TitleWidgetClass;

  // [New] MMO 스타일 접속 UI (캐릭터 선택)
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
  TSubclassOf<class UUserWidget> CharacterSelectWidgetClass;

  // [New] 캐릭터 생성 UI 클래스
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
  TSubclassOf<class UUserWidget> CharacterCreationWidgetClass;

  UPROPERTY(BlueprintReadOnly, Category = "UI")
  TObjectPtr<class UUserWidget> CurrentWidget;

  // [New] 타이틀 UI 표시
  UFUNCTION(BlueprintCallable, Category = "UI")
  void ShowTitleUI();

  // [New] 타이틀 -> 캐릭터 선택 화면으로 전환
  UFUNCTION(BlueprintCallable, Category = "UI")
  void GoToCharacterSelect();

  // [New] 캐릭터 생성 화면으로 전환
  UFUNCTION(BlueprintCallable, Category = "UI")
  void StartCharacterCreation(int32 SlotIndex);

  // [New] 캐릭터 생성 완료/취소 시 복귀
  UFUNCTION(BlueprintCallable, Category = "UI")
  void OnCharacterCreationFinished();

  // [New] 캐릭터 선택 UI 표시 (내부용)
  void ShowCharacterSelectUI();

  // [New] 서버에 캐릭터 스폰 요청
  UFUNCTION(Server, Reliable, BlueprintCallable)
  void ServerSpawnCharacter(int32 SlotIndex);

  // [New] 서버에 게임 시작 요청 (맵 이동)
  UFUNCTION(Server, Reliable, BlueprintCallable)
  void ServerStartGame(const FString &MapName);

  // [New] 스폰 완료 후 클라이언트 처리 (UI 닫기 등)
  UFUNCTION(Client, Reliable)
  void ClientOnSpawnFinished();

  // [New] 클라이언트의 장비 정보를 서버로 동기화 (접속 시 호출)
  UFUNCTION(Server, Reliable, BlueprintCallable)
  void
  ServerSyncEquipment(const TArray<struct FEquipmentSaveData> &EquipmentData);

  // [New] 클라이언트의 인벤토리 정보를 서버로 동기화 (접속 시 호출)
  UFUNCTION(Server, Reliable, BlueprintCallable)
  void ServerSyncInventory(const TArray<struct FInventorySaveData> &InventoryData);

  // [New] 클라이언트의 위치 정보를 서버로 동기화 (접속 시 호출)
  UFUNCTION(Server, Reliable, BlueprintCallable)
  void ServerSyncTransform(const FTransform &Transform);

  // === [New] 사망 및 리스폰 시스템 ===
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Death")
  TSubclassOf<class UUserWidget> GameOverWidgetClass;

  UFUNCTION(Client, Reliable)
  void ShowGameOverUI();

  UFUNCTION(Server, Reliable, BlueprintCallable)
  void ServerRespawnPlayer(bool bInPlace);

  // === [New] 1:1 결투(Duel) 시스템 ===
  UPROPERTY(BlueprintReadOnly, Replicated, Category = "Non|Duel")
  TObjectPtr<ANonPlayerController> CurrentDuelOpponent = nullptr;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Non|Duel")
  float DuelMaxDistance = 2000.f; // 결투 이탈 최대 거리

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Non|Duel")
  float DuelDuration = 300.f; // 결투 지속 시간 (5분)

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Non|Duel")
  FName DuelingTagName = FName(TEXT("State.Combat.Dueling"));

  // 결투 신청 보내기 (서버로 전송)
  UFUNCTION(Server, Reliable, WithValidation)
  void Server_RequestDuel(ANonPlayerController* TargetPlayer);

  UFUNCTION(Server, Reliable, WithValidation)
  void Server_RequestDuelByCharacter(ANonCharacterBase* TargetChar);

  // 결투 신청을 클라이언트에게 띄우기
  UFUNCTION(Client, Reliable)
  void Client_ReceiveDuelRequest(ANonPlayerController* Requester);

  // 결투 신청 수락 (서버로 전송)
  UFUNCTION(Server, Reliable, WithValidation)
  void Server_AcceptDuel(ANonPlayerController* Requester);

  // 결투 거절 (서버로 전송)
  UFUNCTION(Server, Reliable, WithValidation)
  void Server_DeclineDuel(ANonPlayerController* Requester);

  // 결투 거절 알림을 신청자에게 전송
  UFUNCTION(Client, Reliable)
  void Client_DeclineDuelNotification(const FString& RefuserName);

  // 결투 종료 (서버에서 각 클라이언트에 알림)
  UFUNCTION(NetMulticast, Reliable)
  void Multicast_EndDuel(ANonPlayerController* Winner, ANonPlayerController* Loser, bool bDraw = false);

  void CheckDuelDistanceAndRules();

  FTimerHandle DuelDistanceCheckTimerHandle;

  // [New] 선택한 슬롯 저장 (Replicated)
  UPROPERTY(Replicated)
  int32 SelectedSlotIndex = -1;

  // ── [Party & Trade] 파티 및 1:1 개인 거래 컴포넌트 ──
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
  TObjectPtr<UPartyComponent> PartyComponent = nullptr;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
  TObjectPtr<UTradeComponent> TradeComponent = nullptr;

  UFUNCTION(BlueprintCallable, Category = "Party")
  UPartyComponent* GetPartyComponent();

  UFUNCTION(BlueprintCallable, Category = "Trade")
  UTradeComponent* GetTradeComponent();

  // ── [Player Interaction Menu] 플레이어 상호작용 컨텍스트 메뉴 ──
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Interaction")
  TSubclassOf<class UUserWidget> PlayerInteractionMenuWidgetClass;

  UPROPERTY(Transient)
  TObjectPtr<class UPlayerInteractionMenuWidget> PlayerInteractionMenuWidget = nullptr;

  UFUNCTION(BlueprintCallable, Category = "Interaction")
  void ShowPlayerInteractionMenu(ANonCharacterBase* TargetCharacter);

  UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
  void OnShowPlayerInteractionMenu(ANonCharacterBase* TargetCharacter, const FString& TargetNickname);

  // ── [Interaction Request Notification Popup] 파티/거래/결투 요청 알림 위젯 ──
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Interaction")
  TSubclassOf<class UUserWidget> InteractionRequestWidgetClass;

  UPROPERTY(Transient)
  TObjectPtr<class UInteractionRequestWidget> InteractionRequestWidget = nullptr;

  UFUNCTION(BlueprintCallable, Category = "Interaction")
  void ShowInteractionRequest(EInteractionRequestType RequestType, const FString& RequesterName, ANonPlayerController* RequesterPC, float TimeoutSeconds = 15.f);

  UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
  void OnShowInteractionRequest(EInteractionRequestType RequestType, const FString& RequesterName);

  // ── [직통 RPC] 파티 초대 및 거래 신청 (PlayerController 고속도로) ──
  UPROPERTY()
  TWeakObjectPtr<ANonPlayerController> PendingPartyInviterPC = nullptr;

  UPROPERTY()
  TWeakObjectPtr<ANonPlayerController> PendingTradeRequesterPC = nullptr;

  UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Party")
  void Server_SendPartyInviteDirect(ANonCharacterBase* TargetChar);

  UFUNCTION(Client, Reliable)
  void Client_ReceivePartyInviteDirect(const FString& InviterNickname, ANonPlayerController* InviterPC);

  UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Party")
  void Server_RespondPartyInviteDirect(bool bAccept);

  UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Trade")
  void Server_SendTradeRequestDirect(ANonCharacterBase* TargetChar);

  UFUNCTION(Client, Reliable)
  void Client_ReceiveTradeRequestDirect(const FString& RequesterNickname, ANonPlayerController* RequesterPC);

  UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Trade")
  void Server_RespondTradeRequestDirect(bool bAccept);

  // ── [채팅 슬래시 명령어용 RPC] ──
  UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Party")
  void Server_SendPartyInviteByNickname(const FString& TargetNickname);

  UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Party")
  void Server_LeavePartyDirect();

  // ── [전역 단축키] 파티/거래/결투 요청 Y(수락) / N(거절) 처리 ──
  UFUNCTION()
  void OnHotkeyAcceptRequest();

  UFUNCTION()
  void OnHotkeyDeclineRequest();

  bool IsChatFocused() const;

  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;
};
