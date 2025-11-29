#include "Core/NonPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"

#include "Equipment/EquipmentComponent.h"
#include "Character/NonCharacterBase.h"
#include "Core/BPC_UIManager.h"
#include "UI/QuickSlotManager.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWidget.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"


// IMC에서 액션을 이름으로 찾아오는 헬퍼 (UE5.5)
static const UInputAction* FindActionInIMC(const UInputMappingContext* IMC, FName ActionName)
{
    if (!IMC) return nullptr;

    //반환은 TConstArrayView<FEnhancedActionKeyMapping>
    const TConstArrayView<FEnhancedActionKeyMapping> Mappings = IMC->GetMappings();

    for (const FEnhancedActionKeyMapping& Map : Mappings)
    {
        if (Map.Action && Map.Action->GetFName() == ActionName)
        {
            return Map.Action.Get(); // TObjectPtr<const UInputAction> → const UInputAction*
        }
    }
    return nullptr;
}

// 이름으로 찾아 바인딩(없으면 로그만)
template<typename UserClass, typename FuncType>
static void BindIfFound(UEnhancedInputComponent* EIC, const UInputMappingContext* IMC, const TCHAR* ActionName, ETriggerEvent Event, UserClass* Obj, FuncType Func)
{
    if (!EIC || !IMC) return;

    if (const UInputAction* IA = FindActionInIMC(IMC, FName(ActionName)))
    {
        EIC->BindAction(IA, Event, Obj, Func);
    }
    else
    {
    }
}

ANonPlayerController::ANonPlayerController()
{
    bShowMouseCursor = false;
}

void ANonPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (ULocalPlayer* LP = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
        {
            if (IMC_Default)     Subsys->AddMappingContext(IMC_Default, 0);
            if (IMC_QuickSlots)  Subsys->AddMappingContext(IMC_QuickSlots, 1);
        }
    }

    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().SetDragTriggerDistance(1);
    }
}

void ANonPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    CachedChar = Cast<ANonCharacterBase>(InPawn);
    CachedQuick = (InPawn ? InPawn->FindComponentByClass<UQuickSlotManager>() : nullptr);
}

TSharedPtr<SViewport> ANonPlayerController::GetGameViewportSViewport(UWorld* World)
{
    if (!World) return nullptr;
    if (UGameViewportClient* GVC = World->GetGameViewport())
    {
        return GVC->GetGameViewportWidget();
    }
    return nullptr;
}

void ANonPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    if (FSlateApplication::IsInitialized() && FSlateApplication::Get().IsDragDropping())
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetWidgetToFocus(nullptr);
        SetInputMode(Mode);

        if (TSharedPtr<SViewport> VP = GetGameViewportSViewport(GetWorld()))
        {
            const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
            FSlateApplication::Get().SetUserFocus(UserIdx, StaticCastSharedPtr<SWidget>(VP), EFocusCause::SetDirectly);
            FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
        }
    }
}

void ANonPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EIC || !IMC_Default) return;

    // 기본 IMC에서 자동 바인딩
    BindIfFound(EIC, IMC_Default, TEXT("IA_Move"), ETriggerEvent::Triggered, this, &ThisClass::OnMove);
    BindIfFound(EIC, IMC_Default, TEXT("IA_Look"), ETriggerEvent::Triggered, this, &ThisClass::OnLook);

    BindIfFound(EIC, IMC_Default, TEXT("IA_Jump"), ETriggerEvent::Started, this, &ThisClass::OnJumpStart);
    BindIfFound(EIC, IMC_Default, TEXT("IA_Jump"), ETriggerEvent::Completed, this, &ThisClass::OnJumpStop);

    BindIfFound(EIC, IMC_Default, TEXT("IA_Attack"), ETriggerEvent::Started, this, &ThisClass::OnAttack);
    BindIfFound(EIC, IMC_Default, TEXT("IA_Inventory"), ETriggerEvent::Started, this, &ThisClass::OnInventory);
    BindIfFound(EIC, IMC_Default, TEXT("IA_CharacterWindow"), ETriggerEvent::Started, this, &ThisClass::OnToggleCharacterWindow);
    BindIfFound(EIC, IMC_Default, TEXT("IA_SkillWindow"), ETriggerEvent::Started, this, &ThisClass::OnToggleSkillWindow);
    BindIfFound(EIC, IMC_Default, TEXT("IA_ToggleArmed"), ETriggerEvent::Started, this, &ThisClass::OnToggleArmed);

    // Guard: Started/Completed/Canceled
    BindIfFound(EIC, IMC_Default, TEXT("IA_Guard"), ETriggerEvent::Started, this, &ThisClass::OnGuardPressed);
    BindIfFound(EIC, IMC_Default, TEXT("IA_Guard"), ETriggerEvent::Completed, this, &ThisClass::OnGuardReleased);
    BindIfFound(EIC, IMC_Default, TEXT("IA_Guard"), ETriggerEvent::Canceled, this, &ThisClass::OnGuardReleased);

    // 커서 토글(선택)
    BindIfFound(EIC, IMC_Default, TEXT("IA_CursorToggle"), ETriggerEvent::Started, this, &ThisClass::ToggleCursorLook);

    //Dodge
    BindIfFound(EIC, IMC_Default, TEXT("IA_Dodge"), ETriggerEvent::Started, this, &ThisClass::OnDodge);

    // 퀵슬롯 IMC가 있으면 자동 바인딩
    if (IMC_QuickSlots)
    {
        BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_1"), ETriggerEvent::Started, this, &ThisClass::OnQS1);
        BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_2"), ETriggerEvent::Started, this, &ThisClass::OnQS2);
        BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_3"), ETriggerEvent::Started, this, &ThisClass::OnQS3);
        BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_4"), ETriggerEvent::Started, this, &ThisClass::OnQS4);
        BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_5"), ETriggerEvent::Started, this, &ThisClass::OnQS5);
        BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_6"), ETriggerEvent::Started, this, &ThisClass::OnQS6);
        BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_7"), ETriggerEvent::Started, this, &ThisClass::OnQS7);
        BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_8"), ETriggerEvent::Started, this, &ThisClass::OnQS8);
        BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_9"), ETriggerEvent::Started, this, &ThisClass::OnQS9);
        BindIfFound(EIC, IMC_QuickSlots, TEXT("IA_QS_0"), ETriggerEvent::Started, this, &ThisClass::OnQS0);
    }

    // IA_Move 액션 포인터 캐시
    if (const UInputAction* FoundMove = FindActionInIMC(IMC_Default, FName(TEXT("IA_Move"))))
    {
        IA_MoveCached = FoundMove;
    }
}

//QuickSlot
void ANonPlayerController::OnQS0(const FInputActionInstance&) { HandleQuickSlot(10); }
void ANonPlayerController::OnQS1(const FInputActionInstance&) { HandleQuickSlot(1); }
void ANonPlayerController::OnQS2(const FInputActionInstance&) { HandleQuickSlot(2); }
void ANonPlayerController::OnQS3(const FInputActionInstance&) { HandleQuickSlot(3); }
void ANonPlayerController::OnQS4(const FInputActionInstance&) { HandleQuickSlot(4); }
void ANonPlayerController::OnQS5(const FInputActionInstance&) { HandleQuickSlot(5); }
void ANonPlayerController::OnQS6(const FInputActionInstance&) { HandleQuickSlot(6); }
void ANonPlayerController::OnQS7(const FInputActionInstance&) { HandleQuickSlot(7); }
void ANonPlayerController::OnQS8(const FInputActionInstance&) { HandleQuickSlot(8); }
void ANonPlayerController::OnQS9(const FInputActionInstance&) { HandleQuickSlot(9); }

void ANonPlayerController::OnMove(const FInputActionValue& Value)
{
    if (CachedChar) CachedChar->MoveInput(Value);
}
void ANonPlayerController::OnLook(const FInputActionValue& Value)
{
    if (CachedChar) CachedChar->LookInput(Value);
}
void ANonPlayerController::OnJumpStart(const FInputActionValue& /*Value*/)
{
    if (!CachedChar)
        return;

    // 캐릭터에서 ASC 꺼내기
    UAbilitySystemComponent* ASC = nullptr;
    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(CachedChar))
    {
        ASC = ASI->GetAbilitySystemComponent();
    }

    // Dodge 태그 있으면 점프 막기
    if (ASC)
    {
        // Dodge / Attack / Combo 중 하나라도 있으면 점프 막기
            static const FGameplayTag DodgeTag =
            FGameplayTag::RequestGameplayTag(TEXT("State.Dodge"));

        static const FGameplayTag AttackTag =
            FGameplayTag::RequestGameplayTag(TEXT("State.Attack"));

        static const FGameplayTag ComboActiveTag =
            FGameplayTag::RequestGameplayTag(TEXT("Ability.Active.Combo"));

        if (ASC->HasMatchingGameplayTag(DodgeTag) ||
            ASC->HasMatchingGameplayTag(AttackTag) ||
            ASC->HasMatchingGameplayTag(ComboActiveTag))
        {
            return; // 점프 안 함
        }
    }

    // Dodge 중이 아니면 평소처럼 점프
    CachedChar->Jump();
}

void ANonPlayerController::OnJumpStop(const FInputActionValue& /*Value*/)
{
    if (CachedChar) CachedChar->StopJumping();
}

void ANonPlayerController::OnAttack(const FInputActionValue& Value)
{
    if (bShowMouseCursor)
    {
        bool bOverUI = false;
        if (APawn* P = GetPawn())
        {
            if (UBPC_UIManager* UIMan = P->FindComponentByClass<UBPC_UIManager>())
            {
                bOverUI = UIMan->IsCursorOverUI();
            }
        }

        if (!bOverUI)
        {
            SetShowMouseCursor(false);
            FInputModeGameOnly GameOnly;
            SetInputMode(GameOnly);
            bEnableClickEvents = false;
            bEnableMouseOverEvents = false;
        }
        return;
    }

    if (CachedChar) CachedChar->HandleAttackInput(Value);
}


void ANonPlayerController::OnToggleArmed(const FInputActionValue& /*Value*/)
{
    if (!CachedChar) return;

    if (UEquipmentComponent* Eq = CachedChar->FindComponentByClass<UEquipmentComponent>())
    {
        if (!Eq->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain))
        {
            if (CachedChar->IsArmed())
                CachedChar->SetArmed(false);
            return;
        }
    }

    CachedChar->SetArmed(!CachedChar->IsArmed());
}

void ANonPlayerController::OnInventory()
{
    if (APawn* P = GetPawn())
    {
        if (UBPC_UIManager* UIMan = P->FindComponentByClass<UBPC_UIManager>())
        {
            UIMan->ToggleInventory();
        }
        else
        {
        }
    }
}

void ANonPlayerController::OnToggleSkillWindow()
{
    if (APawn* P = GetPawn())
    {
        if (UBPC_UIManager* UIMan = P->FindComponentByClass<UBPC_UIManager>())
        {
            UIMan->ToggleSkillWindow();
            return;
        }
    }
}

void ANonPlayerController::ToggleCursorLook()
{
    if (bShowMouseCursor)
    {
        bCursorFree = false;
        SetShowMouseCursor(false);
        FInputModeGameOnly GameOnly;
        SetInputMode(GameOnly);
        bEnableClickEvents = false;
        bEnableMouseOverEvents = false;
        return;
    }

    bCursorFree = true;
    SetShowMouseCursor(true);

    FInputModeGameAndUI GameAndUI;
    GameAndUI.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    GameAndUI.SetHideCursorDuringCapture(false);
    GameAndUI.SetWidgetToFocus(nullptr);
    SetInputMode(GameAndUI);

    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void ANonPlayerController::OnToggleCharacterWindow()
{
    if (APawn* P = GetPawn())
    {
        if (UBPC_UIManager* UIMan = P->FindComponentByClass<UBPC_UIManager>())
        {
            UIMan->ToggleCharacter();
        }
        else
        {
        }
    }
}

void ANonPlayerController::OnGuardPressed(const FInputActionInstance& /*Instance*/)
{
    if (CachedChar)
    {
        CachedChar->GuardPressed();
    }
}

void ANonPlayerController::OnGuardReleased(const FInputActionInstance& /*Instance*/)
{
    if (CachedChar)
    {
        CachedChar->GuardReleased();
    }
}

void ANonPlayerController::HandleQuickSlot(int32 OneBased)
{
    int32 ZeroBased = -1;
    if (OneBased == 10)      ZeroBased = 9;
    else if (OneBased >= 1 && OneBased <= 9) ZeroBased = OneBased - 1;
    else { return; }

    if (!CachedQuick)
    {
        if (APawn* P = GetPawn())
        {
            CachedQuick = P->FindComponentByClass<UQuickSlotManager>();
        }
    }

    if (CachedQuick)
    {
        CachedQuick->UseQuickSlot(ZeroBased);
    }
    else
    {
    }
}

void ANonPlayerController::OnDodge(const FInputActionValue& Value)
{
    if (!CachedChar) return;

    FVector2D Input2D = FVector2D::ZeroVector;

    // 1) IA_Dodge 값
    if (Value.GetValueType() == EInputActionValueType::Axis2D)
    {
        Input2D = Value.Get<FVector2D>();
    }

    // 2) IA_Move 값
    if (Input2D.IsNearlyZero() && InputComponent)
    {
        if (const UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
        {
            if (IA_MoveCached)
            {
                const FInputActionValue MoveVal = EIC->GetBoundActionValue(IA_MoveCached);
                if (MoveVal.GetValueType() == EInputActionValueType::Axis2D)
                {
                    Input2D = MoveVal.Get<FVector2D>();
                }
            }
        }
    }

    // 3) 최근 이동 입력
    if (Input2D.IsNearlyZero())
    {
        const FVector Last = CachedChar->GetLastMovementInputVector();
        if (!Last.IsNearlyZero())
        {
            const FRotator CtrlYaw(0.f, GetControlRotation().Yaw, 0.f);
            const FVector Fwd = FRotationMatrix(CtrlYaw).GetUnitAxis(EAxis::X);
            const FVector Right = FRotationMatrix(CtrlYaw).GetUnitAxis(EAxis::Y);

            const FVector L2D = Last.GetSafeNormal2D();
            Input2D.X = FVector::DotProduct(L2D, Fwd);
            Input2D.Y = FVector::DotProduct(L2D, Right);
        }
    }

    // 4) 속도 방향
    if (Input2D.IsNearlyZero())
    {
        const FVector Vel = CachedChar->GetVelocity();
        if (Vel.SizeSquared2D() > KINDA_SMALL_NUMBER)
        {
            const FRotator CtrlYaw(0.f, GetControlRotation().Yaw, 0.f);
            const FVector Fwd = FRotationMatrix(CtrlYaw).GetUnitAxis(EAxis::X);
            const FVector Right = FRotationMatrix(CtrlYaw).GetUnitAxis(EAxis::Y);

            const FVector V2D = FVector(Vel.X, Vel.Y, 0.f).GetSafeNormal();
            Input2D.X = FVector::DotProduct(V2D, Fwd);
            Input2D.Y = FVector::DotProduct(V2D, Right);
        }
    }

    // 5) 정규화(대각/직각 유지)
    if (!Input2D.IsNearlyZero())
    {
        const float ax = FMath::Abs(Input2D.X);
        const float ay = FMath::Abs(Input2D.Y);
        const float maxa = FMath::Max(ax, ay);
        if (maxa > SMALL_NUMBER)
        {
            Input2D /= maxa;
        }
    }

    // === 여기까지는 방향 계산용 (나중에 GA_Dodge에서 쓰고 싶으면 캐릭터에 저장) ===

    // 지금은 GA_Dodge가 Char->GetLastMovementInputVector() 기준으로 방향 계산하니까
    // Input2D는 그냥 무시하고, Ability만 실행해도 됨.
    CachedChar->TryDodge();
}