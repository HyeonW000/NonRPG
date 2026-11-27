#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UI/Chat/ChatTypes.h"
#include "UI/InteractionRequestWidget.h"
#include "Skill/SkillTypes.h" // EJobClass
#include "System/NonSaveGame.h" // 입장 데이터에 담는 세이브 구조체
#include "NonPlayerController.generated.h"

class UNonUIManagerComponent;
class ANonCharacterBase;
class UQuickSlotManager;
class UPartyComponent;
class UTradeComponent;
class ULoadingScreenWidget;

// 입장 데이터: 클라이언트가 자기 세이브에서 읽어 서버로 한 번에 보내는 캐릭터 정보 (NON-DIS-2).
// 세이브는 각 플레이어 PC에만 있으므로 서버는 이 값을 받아 검사한 뒤 적용한다 (ANonCharacterBase::ApplySpawnSaveData).
// 퀵슬롯은 각자 PC에만 있는 정보라 여기 없음 (클라이언트가 직접 복원).
USTRUCT(BlueprintType)
struct FNonSpawnCharacterData {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly)
  bool bHasSave = false;

  UPROPERTY(BlueprintReadOnly)
  FString PlayerName;

  UPROPERTY(BlueprintReadOnly)
  EJobClass JobClass = EJobClass::None;

  UPROPERTY(BlueprintReadOnly)
  int32 Level = 1;

  UPROPERTY(BlueprintReadOnly)
  int32 EXP = 0;

  UPROPERTY(BlueprintReadOnly)
  float CurrentHP = 0.f;

  UPROPERTY(BlueprintReadOnly)
  float CurrentMP = 0.f;

  UPROPERTY(BlueprintReadOnly)
  int32 Gold = 0;

  UPROPERTY(BlueprintReadOnly)
  int32 SkillPoints = 0;

  UPROPERTY(BlueprintReadOnly)
  TArray<FSkillLevelSaveEntry> SkillLevels;

  UPROPERTY(BlueprintReadOnly)
  TArray<FInventorySaveData> InventoryItems;

  UPROPERTY(BlueprintReadOnly)
  TArray<FEquipmentSaveData> EquippedItems;

  UPROPERTY(BlueprintReadOnly)
  FName SavedMapName = NAME_None;

  UPROPERTY(BlueprintReadOnly)
  FTransform PlayerTransform;
};

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

  // [Debug] 콘솔 명령 (Shipping 빌드에서는 아무 동작 안 함). 가장 가까운 보스에 적용, 서버에서 실행
  //   NonBossGroggy              : 즉시 그로기
  //   NonBossBreakPart [부위이름] : 즉시 부위 파괴 (이름 생략 시 아직 안 부서진 첫 부위)
  UFUNCTION(Exec)
  void NonBossGroggy();

  UFUNCTION(Exec)
  void NonBossBreakPart(const FString &PartName);

  UFUNCTION(Server, Reliable)
  void Server_DebugBossGroggy();

  UFUNCTION(Server, Reliable)
  void Server_DebugBossBreakPart(FName PartName);


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
  // 잡힘 등으로 행동 입력(이동, 공격, 회피, 가드, 점프, 퀵슬롯, 상호작용, 무기 전환)을 막아야 하는 상태인지
  bool IsActionInputBlocked() const;

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

  // ── [멀티플레이 상호작용 Server RPC] ──
  // 클라이언트가 F키를 눌렀을 때 서버에게 시체 루팅, 대화 등의 상호작용 실행을 요청합니다.
  UFUNCTION(Server, Reliable, WithValidation)
  void Server_Interact(AActor* TargetActor);

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

  // 이미 루팅을 완료한 시체 목록 (개인 루팅 후 외곽선/프롬프트 자동 해제용)
  UPROPERTY(Transient)
  TSet<TWeakObjectPtr<AActor>> LootedCorpses;

public:
  UFUNCTION(BlueprintPure, Category = "Interaction")
  bool HasLootedCorpse(const AActor* Corpse) const { return LootedCorpses.Contains(Corpse); }

  UFUNCTION(Client, Reliable)
  void Client_OnCorpseLooted(AActor* LootedCorpse);

protected:
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

  // 캐릭터 스폰 요청 (로컬 플레이어에서 호출): 내 PC의 세이브를 읽어 서버로 전달.
  // 예전 이름을 유지하지만 RPC가 아니라 로컬 함수이며, 실제 RPC는 Server_SpawnCharacterWithData.
  UFUNCTION(BlueprintCallable)
  void ServerSpawnCharacter(int32 SlotIndex);

  UFUNCTION(Server, Reliable)
  void Server_SpawnCharacterWithData(int32 SlotIndex, const FNonSpawnCharacterData &Data);

  // 이 PC(로컬)의 세이브에서 스폰 정보 읽기. 세이브가 없으면 false
  bool LoadLocalSpawnData(int32 SlotIndex, FNonSpawnCharacterData &OutData) const;

  // [New] 서버에 게임 시작 요청 (맵 이동)
  UFUNCTION(Server, Reliable, BlueprintCallable)
  void ServerStartGame(const FString &MapName);

  // [New] 스폰 완료 후 클라이언트 처리 (UI 닫기 등)
  UFUNCTION(Client, Reliable)
  void ClientOnSpawnFinished();

  /** 로컬 세이브의 퀵슬롯을 복원 (퀵슬롯은 각자 PC에만 있음). 인벤토리·스킬 복제가 도착한 뒤 호출 */
  void RestoreLocalQuickSlots();

  /** 서버가 "곧 저장해" 요청 (레벨업 등). 세이브는 각자 PC에 있으므로 서버는 직접 저장할 수 없다 */
  UFUNCTION(Client, Reliable)
  void ClientRequestSave();

private:
  /** 입장 후 서버가 적용한 세이브가 복제로 도착했는지 확인 -> 퀵슬롯 복원 -> 저장 시작 */
  void CheckSaveApplied();
  FTimerHandle SaveAppliedTimer;
  int32 SaveAppliedChecks = 0;

public:

  // === [New] 사망 및 리스폰 시스템 ===
  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Death")
  TSubclassOf<class UUserWidget> GameOverWidgetClass;

  // bCanReviveInPlace: 서버가 확인한 부활 부적 보유 여부. false면 "제자리 부활" 버튼 비활성화
  UFUNCTION(Client, Reliable)
  void ShowGameOverUI(bool bCanReviveInPlace);

  UFUNCTION(Server, Reliable, BlueprintCallable)
  void ServerRespawnPlayer(bool bInPlace);

  // 현재 폰의 인벤토리에서 부활 부적 슬롯을 찾음 (없으면 INDEX_NONE). 서버 기준으로 사용
  int32 FindResurrectionAmuletSlot() const;

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

  UFUNCTION(Client, Reliable)
  void Client_ShowPlayerInteractionMenu(ANonCharacterBase* TargetCharacter);

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

  // ── [던전/레벨 이동] 워프레임 방식: 파티장 호스팅 및 파티원 동반 접속 시스템 ──
  UFUNCTION(Client, Reliable)
  void Client_HostDungeon(const FString& InMapName);

  UFUNCTION(Client, Reliable)
  void Client_JoinDungeon(const FString& HostAddress, float DelaySeconds);

  UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Level")
  void Server_RequestPartyDungeonTravel(const FString& InMapName);

  // ── [로딩 연출] 레벨 전환 시 화면 암전(Fade) 및 로딩창 ──
  UFUNCTION(BlueprintCallable, Category = "UI|Loading")
  void ShowLoadingScreen(const FString& LoadingMessage = TEXT("던전으로 이동하는 중입니다..."));

  UFUNCTION(BlueprintCallable, Category = "UI|Loading")
  void HideLoadingScreen();

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Loading")
  TSubclassOf<class ULoadingScreenWidget> LoadingWidgetClass = nullptr;

  UPROPERTY()
  TObjectPtr<class ULoadingScreenWidget> LoadingWidget = nullptr;

  bool IsChatFocused() const;

  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;
};
