#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "NonPlayerController.generated.h"

class UNonUIManagerComponent;
class ANonCharacterBase;
class UQuickSlotManager;

UCLASS()
class NON_API ANonPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ANonPlayerController();

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void PlayerTick(float DeltaTime) override;

    static TSharedPtr<class SViewport> GetGameViewportSViewport(UWorld* World);

    // IMC만 꽂으면 자동 바인딩
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    class UInputMappingContext* IMC_Default;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    class UInputMappingContext* IMC_QuickSlots;

    // --- [Refactor] 명시적 Input Action 바인딩 ---
    // 캐릭터 이동/전투 관련
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_Move;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_Look;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_Jump;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_Attack;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_Dodge;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_Guard;

    // UI 관련
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_Inventory;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_SkillWindow;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_CharacterWindow;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_Interact;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_ESC;
    
    // ToggleArmed (Character에도 있지만 UI/Controller 레벨에서 처리할 경우)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_ToggleArmed;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_CursorToggle = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Actions")
    class UInputAction* IA_Zoom = nullptr;

    // 캐시
    UPROPERTY() ANonCharacterBase* CachedChar = nullptr;
    UPROPERTY() UQuickSlotManager* CachedQuick = nullptr;

    // 입력 핸들러
    void OnMove(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);
    void OnZoom(const FInputActionValue& Value);
    void OnJumpStart(const FInputActionValue& Value);
    void OnJumpStop(const FInputActionValue& Value);
    void OnAttack(const FInputActionValue& Value);
    void OnInventory();
    void OnToggleSkillWindow();
    void OnToggleCharacterWindow();
    void OnToggleArmed(const FInputActionValue& Value);
    void OnGuardPressed(const FInputActionInstance& Instance);
    void OnGuardReleased(const FInputActionInstance& Instance);
    void OnDodge(const FInputActionValue& Value);
    void OnInteract(const FInputActionInstance& Instance);
    void OnEsc(const FInputActionInstance& Instance);

private:

    bool bCursorFree = false;
    void ToggleCursorLook();

    // 퀵슬롯 공통 처리
    UFUNCTION()
    void HandleQuickSlot(int32 OneBased);

    // 퀵슬롯 래퍼들 (0 키 = 10번째)
    UFUNCTION() void OnQS0(const FInputActionInstance& Instance);
    UFUNCTION() void OnQS1(const FInputActionInstance& Instance);
    UFUNCTION() void OnQS2(const FInputActionInstance& Instance);
    UFUNCTION() void OnQS3(const FInputActionInstance& Instance);
    UFUNCTION() void OnQS4(const FInputActionInstance& Instance);
    UFUNCTION() void OnQS5(const FInputActionInstance& Instance);
    UFUNCTION() void OnQS6(const FInputActionInstance& Instance);
    UFUNCTION() void OnQS7(const FInputActionInstance& Instance);
    UFUNCTION() void OnQS8(const FInputActionInstance& Instance);
    UFUNCTION() void OnQS9(const FInputActionInstance& Instance);

    // Move 입력 액션을 캐시해 두면 Dodge 시점에 현재 Move 값을 바로 읽을 수 있음
    UPROPERTY()
    TObjectPtr<const UInputAction> IA_MoveCached = nullptr;

    // 현재 바라보고 있는 상호작용 타겟
    TWeakObjectPtr<AActor> CurrentInteractTarget;

    void UpdateInteractFocus(float DeltaTime);
};
