#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillTypes.h"
#include "UI/UITypes.h"
#include "Components/ActorComponent.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "TimerManager.h"
#include "Data/DialogueData.h"
#include "NonUIManagerComponent.generated.h"

class UInGameHUD;
class UUserWidget;
class UInventoryComponent;
class UEquipmentComponent;
class UCharacterWindowWidget;
class USkillWindowWidget;
class UAbilitySystemComponent; // [New]

UCLASS(ClassGroup = (UI), meta = (BlueprintSpawnableComponent))
class NON_API UNonUIManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UNonUIManagerComponent();
    virtual void BeginPlay() override;

    // ----- HUD -----
    void InitHUD();
    UInGameHUD* GetInGameHUD() const { return InGameHUD; }

    // ----- HUD bridge updates -----
    void UpdateHP(float Current, float Max);
    void UpdateMP(float Current, float Max);
    void UpdateSP(float Current, float Max);
    void UpdateEXP(float Current, float Max);
    void UpdateLevel(int32 NewLevel);
    
    // [New] 타겟 정보 갱신
    void UpdateTargetHUD(class AActor* TargetActor, const FString& Name, float HP, float MaxHP, float Distance);

    // [New] 외부에서 HUD 상태 전체 갱신 요청
    UFUNCTION(BlueprintCallable, Category = "UI")
    void RefreshHUDState();

    // ----- Inventory Window -----
    UFUNCTION(BlueprintCallable) void ToggleInventory();
    UFUNCTION(BlueprintCallable) void ShowInventory();
    UFUNCTION(BlueprintCallable) void HideInventory();
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Inventory")
    bool IsInventoryVisible() const;
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Inventory")
    UUserWidget* GetInventoryWidget() const;

    // ----- Character Window -----
    UFUNCTION(BlueprintCallable) void ToggleCharacter();
    UFUNCTION(BlueprintCallable) void ShowCharacter();
    UFUNCTION(BlueprintCallable) void HideCharacter();
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Character")
    bool IsCharacterVisible() const;
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Character")
    UUserWidget* GetCharacterWidget() const;

    // ----- Common Window Ops -----
    UFUNCTION(BlueprintCallable, Category = "UI|Windows")
    void RegisterWindow(UUserWidget* Window);
    UFUNCTION(BlueprintCallable, Category = "UI|Windows")
    void UnregisterWindow(UUserWidget* Window);
    UFUNCTION(BlueprintCallable, Category = "UI|Windows")
    void BringToFront(UUserWidget* Window);
    UFUNCTION(BlueprintCallable, Category = "UI|Windows")
    void NotifyWindowClosed(UUserWidget* Window);

    //Skill Window
    UFUNCTION(BlueprintCallable) void ToggleSkillWindow();
    UFUNCTION(BlueprintCallable) void ShowSkillWindow();
    UFUNCTION(BlueprintCallable) void HideSkillWindow();

    // ESC 닫기용 (TopMost 1개 닫기, 성공시 true)
    UFUNCTION(BlueprintCallable)
    bool CloseTopWindow();

    // 외부 호환용
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Windows")
    bool IsAnyWindowOpen() const { return IsAnyWindowVisible(); }

    // ========================= Generic Window Management =========================
    UFUNCTION(BlueprintCallable, Category = "UI|Windows")
    void OpenWindow(EGameWindowType Type);

    UFUNCTION(BlueprintCallable, Category = "UI|Windows")
    void CloseWindow(EGameWindowType Type);

    UFUNCTION(BlueprintCallable, Category = "UI|Windows")
    void ToggleWindow(EGameWindowType Type);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Windows")
    bool IsWindowOpen(EGameWindowType Type) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Windows")
    UUserWidget* GetWindow(EGameWindowType Type) const;

    // Hover 추적 (Public이어야 함)
    void NotifyWindowHover(UUserWidget* Window, bool bHovered)
    {
        if (bHovered) ++HoveredWindowCount;
        else          HoveredWindowCount = FMath::Max(0, HoveredWindowCount - 1);
    }
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Windows")
    bool IsCursorOverUI() const { return HoveredWindowCount > 0; }

    UFUNCTION(BlueprintCallable, Category = "UI")
    void RefreshCharacterEquipmentUI();

    // 상호작용 프롬프트 위젯 클래스
    UPROPERTY(EditDefaultsOnly, Category = "UI|Interact")
    TSubclassOf<UUserWidget> InteractPromptClass;

    // 생성된 인스턴스
    UPROPERTY()
    UUserWidget* InteractPromptWidget = nullptr;

    void ShowInteractPrompt(const FText& InLabel);
    void HideInteractPrompt();

    // ========================= Dialogue UI =========================
    UFUNCTION(BlueprintCallable, Category = "UI|Dialogue")
    void ShowDialogue(const FText& NPCName, const FText& DialogueLine);

    UFUNCTION(BlueprintCallable, Category = "UI|Dialogue")
    void HideDialogue();

    // [New] 데이터 테이블 기반 대화 시작
    UFUNCTION(BlueprintCallable, Category = "UI|Dialogue")
    void StartDialogue(class UDataTable* DialogueTable, FName StartingNodeID, const FText& TargetNPCName);

    // [New] 선택지가 없을 때, 다음 대화로 넘어가기 (클릭/버튼 입력 시)
    UFUNCTION(BlueprintCallable, Category = "UI|Dialogue")
    void AdvanceDialogue();

    // [New] 플레이어가 선택지를 골랐을 때 호출됨
    UFUNCTION(BlueprintCallable, Category = "UI|Dialogue")
    void OnDialogueChoiceSelected(const FDialogueChoice& Choice);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Dialogue")
    TSubclassOf<class UNonDialogueWidget> DialogueWidgetClass;

    // [New] 선택지 버튼 위젯 클래스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Dialogue")
    TSubclassOf<class UNonDialogueChoiceWidget> DialogueChoiceWidgetClass;

    UPROPERTY(Transient)
    class UNonDialogueWidget* DialogueWidget = nullptr;

private:
    // 현재 진행 중인 대화 테이블
    UPROPERTY(Transient)
    class UDataTable* CurrentDialogueTable = nullptr;

    // 현재 진행 중인 대화 노드 ID
    FName CurrentDialogueNodeID;

    // 현재 화자 이름
    FText CurrentDialogueNPCName;

protected:
    // 타입에 맞는 위젯 클래스 반환 (기존 프로퍼티 활용)
    TSubclassOf<UUserWidget> GetWindowClass(EGameWindowType Type) const;

    // 생성된 위젯 초기화 (Component 주입 등)
    void SetupWindow(EGameWindowType Type, UUserWidget* Widget);

    // 통합 관리 맵
    UPROPERTY()
    TMap<EGameWindowType, TObjectPtr<UUserWidget>> ManagedWindows;

    void SetUIInputMode(bool bShowCursor = false);
    void SetGameInputMode();
    bool IsAnyWindowVisible() const;

    // 위젯 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|HUD")
    TSubclassOf<UInGameHUD> InGameHUDClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Inventory")
    TSubclassOf<UUserWidget> InventoryWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Character")
    TSubclassOf<UUserWidget> CharacterWidgetClass;

    // 에디터에서 WBP_SkillWindow 지정
    UPROPERTY(EditDefaultsOnly, Category = "UI|Skill")
    TSubclassOf<UUserWidget> SkillWindowClass = nullptr;

    // [New] 직업별 아이콘 (저장된 직업 -> HUD 아이콘 표시용)
    UPROPERTY(EditDefaultsOnly, Category = "UI|Class")
    TMap<EJobClass, UTexture2D*> ClassIcons;

    UPROPERTY(EditDefaultsOnly, Category = "UI|System")
    TSubclassOf<UUserWidget> SystemMenuWidgetClass = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> SkillWindow = nullptr;

    UPROPERTY(Transient)
    USkillWindowWidget* SkillWindowContent = nullptr;

    // [Removed] SkillDataByJob - Now handled by SkillManagerComponent directly
    // UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Skill")
    // TMap<EJobClass, USkillDataAsset*> SkillDataByJob;

    // [New] GAS Attribute 변경 감지 바인딩
    void BindAttributeChangeDelegates(class UAbilitySystemComponent* ASC);

    void OnHPChanged(const struct FOnAttributeChangeData& Data);
    void OnMaxHPChanged(const struct FOnAttributeChangeData& Data);
    void OnMPChanged(const struct FOnAttributeChangeData& Data);
    void OnMaxMPChanged(const struct FOnAttributeChangeData& Data);
    void OnSPChanged(const struct FOnAttributeChangeData& Data);
    void OnMaxSPChanged(const struct FOnAttributeChangeData& Data);
    void OnExpChanged(const struct FOnAttributeChangeData& Data);
    void OnMaxExpChanged(const struct FOnAttributeChangeData& Data);
    void OnLevelChanged(const struct FOnAttributeChangeData& Data);

    bool bAttributeDelegatesBound = false;

private:
    APlayerController* GetPC() const;
    static bool GetWidgetViewportPos(UUserWidget* W, FVector2D& OutViewportPos);

    UFUNCTION()
    void OnJobChanged(EJobClass NewJob);

    void UpdateClassIconFromJob(EJobClass Job);

    bool bUIOpen = false;
    UPROPERTY() UInGameHUD* InGameHUD = nullptr;
    UPROPERTY() TWeakObjectPtr<UUserWidget> InventoryWidget;
    UPROPERTY() TWeakObjectPtr<UUserWidget> CharacterWidget;
    UPROPERTY() bool bInventoryUIInited = false;

    UPROPERTY() TArray<TWeakObjectPtr<UUserWidget>> OpenWindows;
    int32 TopZOrder = 1000;

    FTimerHandle DeferredPosTimerHandle;
    bool bDeferredPosScheduled = false;


    int32 HoveredWindowCount = 0;

    static FVector2D ClampToViewport(const FVector2D& InPx, const FVector2D& ViewportPx, const FVector2D& WindowPx)
    {
        const float X = FMath::Clamp(InPx.X, 0.f, FMath::Max(0.f, ViewportPx.X - WindowPx.X));
        const float Y = FMath::Clamp(InPx.Y, 0.f, FMath::Max(0.f, ViewportPx.Y - WindowPx.Y));
        return FVector2D(X, Y);
    }
};