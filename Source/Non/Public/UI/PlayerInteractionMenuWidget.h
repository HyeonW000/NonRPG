#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerInteractionMenuWidget.generated.h"

class ANonCharacterBase;
class ANonPlayerController;
class UButton;
class UTextBlock;

/**
 * 다른 플레이어와 상호작용(E키) 시 화면에 팝업되는 목록 UI 위젯 베이스 클래스
 */
UCLASS(Blueprintable, BlueprintType)
class NON_API UPlayerInteractionMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPlayerInteractionMenuWidget(const FObjectInitializer& ObjectInitializer);

    /** 메뉴 초기화 (대상 캐릭터 설정 및 이름/직업 텍스트 갱신) */
    UFUNCTION(BlueprintCallable, Category = "PlayerInteraction")
    void InitializeMenu(ANonCharacterBase* InTargetChar);

    /** 메뉴 닫기 및 입력 모드 복구 */
    UFUNCTION(BlueprintCallable, Category = "PlayerInteraction")
    void CloseMenu();

    /** 대상 캐릭터 반환 */
    UFUNCTION(BlueprintPure, Category = "PlayerInteraction")
    ANonCharacterBase* GetTargetCharacter() const { return TargetCharacter.Get(); }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** 대상 캐릭터의 3D 위치를 2D 화면 좌표로 투영하여 메뉴 위치 갱신 */
    void UpdateScreenPosition();

    // ── UMG 바인딩 위젯 (Blueprint WBP에서 이름만 일치시키면 자동 연결) ──
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI")
    TObjectPtr<UWidget> MenuContainer;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI")
    TObjectPtr<UTextBlock> Text_TargetName;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI")
    TObjectPtr<UButton> Button_PartyInvite;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI")
    TObjectPtr<UButton> Button_Trade;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI")
    TObjectPtr<UButton> Button_Duel;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "UI")
    TObjectPtr<UButton> Button_Inspect;

    // ── 키보드 ESC 입력 시 메뉴 닫기 지원 ──
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    // ── 버튼 클릭 핸들러 ──
    UFUNCTION()
    void OnPartyInviteClicked();

    UFUNCTION()
    void OnTradeClicked();

    UFUNCTION()
    void OnDuelClicked();

    UFUNCTION()
    void OnInspectClicked();

    // ── 블루프린트 이벤트 (사운드/애니메이션 및 정보보기 창 연동) ──
    UFUNCTION(BlueprintImplementableEvent, Category = "PlayerInteraction")
    void BP_OnMenuOpened(ANonCharacterBase* TargetChar, const FString& TargetName);

    UFUNCTION(BlueprintImplementableEvent, Category = "PlayerInteraction")
    void BP_OnInspectPlayer(ANonCharacterBase* TargetChar);

private:
    UPROPERTY()
    TWeakObjectPtr<ANonCharacterBase> TargetCharacter;

    UPROPERTY()
    TWeakObjectPtr<ANonPlayerController> TargetPC;
};
