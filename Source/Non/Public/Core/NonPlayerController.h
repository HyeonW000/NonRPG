#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "NonPlayerController.generated.h"

class UBPC_UIManager;
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
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> IMC_Default = nullptr;

    // 퀵슬롯 IMC (선택)
    UPROPERTY(EditDefaultsOnly, Category = "Input|QuickSlots")
    TObjectPtr<UInputMappingContext> IMC_QuickSlots = nullptr;

    // 캐시
    UPROPERTY() ANonCharacterBase* CachedChar = nullptr;
    UPROPERTY() UQuickSlotManager* CachedQuick = nullptr;

    // 입력 핸들러
    void OnMove(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);
    void OnJumpStart(const FInputActionValue& Value);
    void OnJumpStop(const FInputActionValue& Value);
    void OnAttack(const FInputActionValue& Value);
    void OnInventory();
    void OnToggleCharacterWindow();
    void OnToggleArmed(const FInputActionValue& Value);
    void OnGuardPressed(const FInputActionInstance& Instance);
    void OnGuardReleased(const FInputActionInstance& Instance);
    void OnDodge(const FInputActionValue& Value);

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
};
