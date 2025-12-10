#include "Core/NonPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"

#include "Equipment/EquipmentComponent.h"
#include "Character/NonCharacterBase.h"
#include "Core/NonUIManagerComponent.h"
#include "Interaction/NonInteractableInterface.h"
#include "UI/QuickSlot/QuickSlotManager.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWidget.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"

#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"


// 상호작용용 트레이스 채널 (프로젝트 세팅에서 만든 Interact 채널이 GameTraceChannel1 이라는 가정)
static const ECollisionChannel InteractChannel = ECollisionChannel::ECC_GameTraceChannel1;
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

    UpdateInteractFocus(DeltaTime);

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
    
    // Interact
    BindIfFound(EIC, IMC_Default, TEXT("IA_Interact"), ETriggerEvent::Started, this, &ANonPlayerController::OnInteract);

    // ESC
    BindIfFound(EIC, IMC_Default, TEXT("IA_ESC"), ETriggerEvent::Started, this, &ANonPlayerController::OnEsc);

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

    // 태그 있으면 점프 막기
    if (ASC)
    {
        static const FGameplayTag DodgeTag =
            FGameplayTag::RequestGameplayTag(TEXT("State.Dodge"));

        static const FGameplayTag AttackTag =
            FGameplayTag::RequestGameplayTag(TEXT("State.Attack"));

        static const FGameplayTag ComboActiveTag =
            FGameplayTag::RequestGameplayTag(TEXT("Ability.Active.Combo"));

        // ⬇ 새로 추가
        static const FGameplayTag SkillTag =
            FGameplayTag::RequestGameplayTag(TEXT("State.Skill"));

        static const FGameplayTag GuardTag =
            FGameplayTag::RequestGameplayTag(TEXT("State.Guard"));

        if (ASC->HasMatchingGameplayTag(DodgeTag) ||
            ASC->HasMatchingGameplayTag(AttackTag) ||
            ASC->HasMatchingGameplayTag(ComboActiveTag) ||
            ASC->HasMatchingGameplayTag(SkillTag) ||
            ASC->HasMatchingGameplayTag(GuardTag))
        {
            return; // 점프 안 함
        }
    }

    // 점프 막는중 아니면 평소처럼 점프
    CachedChar->Jump();
}

void ANonPlayerController::OnJumpStop(const FInputActionValue& /*Value*/)
{
    if (CachedChar) CachedChar->StopJumping();
}

void ANonPlayerController::OnInteract(const FInputActionInstance& /*Instance*/)
{
    if (!CachedChar)
        return;

    // ASC 꺼내기
    UAbilitySystemComponent* ASC = nullptr;
    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(CachedChar))
    {
        ASC = ASI->GetAbilitySystemComponent();
    }

    if (ASC)
    {
        static const FGameplayTag DodgeTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dodge"));
        static const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attack"));
        static const FGameplayTag SkillTag = FGameplayTag::RequestGameplayTag(TEXT("State.Skill"));
        static const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(TEXT("State.Guard"));
        static const FGameplayTag ComboTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Active.Combo"));

        if (ASC->HasMatchingGameplayTag(DodgeTag) ||
            ASC->HasMatchingGameplayTag(AttackTag) ||
            ASC->HasMatchingGameplayTag(SkillTag) ||
            ASC->HasMatchingGameplayTag(GuardTag) ||
            ASC->HasMatchingGameplayTag(ComboTag))
        {
            // 전투 중이면 상호작용 무시
            return;
        }
    }

    // ==== 여기부터 캡슐 스윕 대신 포커스 타겟 사용 ====

    AActor* Target = CurrentInteractTarget.Get();
    if (!Target)
    {
        // 현재 바라보는 상호작용 대상이 없으면 아무 것도 안 함
        return;
    }

    // 혹시 모를 안전 체크 (인터페이스 달려있는지)
    if (!Target->GetClass()->ImplementsInterface(UNonInteractableInterface::StaticClass()))
    {
        return;
    }

    // 실제 상호작용 호출
    INonInteractableInterface::Execute_Interact(Target, CachedChar);
}


void ANonPlayerController::OnAttack(const FInputActionValue& Value)
{
    if (bShowMouseCursor)
    {
        bool bOverUI = false;
        if (APawn* P = GetPawn())
        {
            if (UNonUIManagerComponent* UIMan = P->FindComponentByClass<UNonUIManagerComponent>())
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
    if (!CachedChar)
        return;

    // 1) 무기 없으면: 무장 상태면 강제로 해제만 하고 끝
    if (UEquipmentComponent* Eq = CachedChar->FindComponentByClass<UEquipmentComponent>())
    {
        if (!Eq->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain))
        {
            if (CachedChar->IsArmed())
            {
                CachedChar->SetArmed(false);
            }
            return;
        }
    }

    // 2) ASC 통해 GA_ToggleWeapon 발동
    UAbilitySystemComponent* ASC = nullptr;
    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(CachedChar))
    {
        ASC = ASI->GetAbilitySystemComponent();
    }
    if (!ASC)
        return;

    static const FGameplayTag ToggleTag =
        FGameplayTag::RequestGameplayTag(TEXT("Ability.ToggleWeapon"));

    FGameplayTagContainer TagContainer;
    TagContainer.AddTag(ToggleTag);

    const bool bSuccess = ASC->TryActivateAbilitiesByTag(TagContainer);

#if WITH_EDITOR
    UE_LOG(LogTemp, Log,
        TEXT("[ToggleWeapon] TryActivateAbilitiesByTag(Ability.ToggleWeapon) => %s"),
        bSuccess ? TEXT("Success") : TEXT("Fail"));
#endif
}

void ANonPlayerController::OnInventory()
{
    if (APawn* P = GetPawn())
    {
        if (UNonUIManagerComponent* UIMan = P->FindComponentByClass<UNonUIManagerComponent>())
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
        if (UNonUIManagerComponent* UIMan = P->FindComponentByClass<UNonUIManagerComponent>())
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
        if (UNonUIManagerComponent* UIMan = P->FindComponentByClass<UNonUIManagerComponent>())
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
    if (!CachedChar)
        return;

    UAbilitySystemComponent* ASC = nullptr;
    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(CachedChar))
    {
        ASC = ASI->GetAbilitySystemComponent();
    }
    if (!ASC)
        return;

    static const FGameplayTag GuardTag =
        FGameplayTag::RequestGameplayTag(TEXT("Ability.Guard"));

    FGameplayTagContainer GuardTags;
    GuardTags.AddTag(GuardTag);

    ASC->TryActivateAbilitiesByTag(GuardTags);
}

void ANonPlayerController::OnGuardReleased(const FInputActionInstance& /*Instance*/)
{
    if (!CachedChar)
        return;

    UAbilitySystemComponent* ASC = nullptr;
    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(CachedChar))
    {
        ASC = ASI->GetAbilitySystemComponent();
    }
    if (!ASC)
        return;

    static const FGameplayTag GuardTag =
        FGameplayTag::RequestGameplayTag(TEXT("Ability.Guard"));

    FGameplayTagContainer GuardTags;
    GuardTags.AddTag(GuardTag);

    ASC->CancelAbilities(&GuardTags);
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

void ANonPlayerController::UpdateInteractFocus(float DeltaTime)
{
    if (!IsLocalController())
        return;

    if (!CachedChar)
        return;

    // 캐릭터 캡슐 정보 가져오기
    UCapsuleComponent* Capsule = CachedChar->GetCapsuleComponent();
    if (!Capsule)
        return;

    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const float TraceDistance = 100.f;

    // 캡슐 중심 기준에서 앞쪽으로 스윕
    FVector Start = CachedChar->GetActorLocation();
    FVector End = Start + CachedChar->GetActorForwardVector() * TraceDistance;

    FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractFocus), false, CachedChar);

    const bool bHit = GetWorld()->SweepSingleByChannel(
        Hit,
        Start,
        End,
        FQuat::Identity,
        InteractChannel,
        Shape,
        Params
    );

//#if WITH_EDITOR
    //const FColor Col = bHit ? FColor::Green : FColor::Red;
    //DrawDebugCapsule(GetWorld(), Start, HalfHeight, Radius, FQuat::Identity, Col, false, 0.f);
    //DrawDebugCapsule(GetWorld(), End, HalfHeight, Radius, FQuat::Identity, Col, false, 0.f);
    //DrawDebugLine(GetWorld(), Start, End, Col, false, 0.f, 0, 1.f);
//#endif

    AActor* NewTarget = nullptr;
    if (bHit && Hit.GetActor() && Hit.GetActor()->Implements<UNonInteractableInterface>())
    {
        NewTarget = Hit.GetActor();
    }

    UNonUIManagerComponent* UI = CachedChar->FindComponentByClass<UNonUIManagerComponent>();

    // 1) 아무 것도 안 맞았으면 → 프롬프트 끄기 + 하이라이트 해제
    if (!NewTarget)
    {
        AActor* OldTarget = CurrentInteractTarget.Get();
        CurrentInteractTarget = nullptr;

        if (OldTarget && OldTarget->GetClass()->ImplementsInterface(UNonInteractableInterface::StaticClass()))
        {
            INonInteractableInterface::Execute_SetInteractHighlight(OldTarget, false);
        }

        if (UI)
        {
            UI->HideInteractPrompt();
        }
        return;
    }

    // 2) 새 타겟으로 변경됐을 때만 처리
    if (NewTarget != CurrentInteractTarget.Get())
    {
        AActor* OldTarget = CurrentInteractTarget.Get();
        CurrentInteractTarget = NewTarget;

        // 이전 타겟 하이라이트 해제
        if (OldTarget && OldTarget->GetClass()->ImplementsInterface(UNonInteractableInterface::StaticClass()))
        {
            INonInteractableInterface::Execute_SetInteractHighlight(OldTarget, false);
        }

        // 새 타겟 하이라이트 on
        if (NewTarget->GetClass()->ImplementsInterface(UNonInteractableInterface::StaticClass()))
        {
            INonInteractableInterface::Execute_SetInteractHighlight(NewTarget, true);
        }

        if (UI)
        {
            FText Label = FText::FromString(TEXT("상호작용"));

            if (NewTarget->GetClass()->ImplementsInterface(UNonInteractableInterface::StaticClass()))
            {
                Label = INonInteractableInterface::Execute_GetInteractLabel(NewTarget);
            }

            UI->ShowInteractPrompt(Label);
        }
    }
}

void ANonPlayerController::OnEsc(const FInputActionInstance& /*Instance*/)
{
    // 1) UI 매니저에게 창 닫기 요청
    if (APawn* P = GetPawn())
    {
        if (UNonUIManagerComponent* UIMan = P->FindComponentByClass<UNonUIManagerComponent>())
        {
            if (UIMan->CloseTopWindow())
            {
                // UI가 하나라도 닫혔으면 여기서 끝 (메뉴 안 띄움)
                return;
            }
        }
    }

    // 2) 닫을 UI가 없었다면 -> 게임 메뉴(시스템 메뉴) 띄우기
    if (APawn* P = GetPawn())
    {
        if (UNonUIManagerComponent* UIMan = P->FindComponentByClass<UNonUIManagerComponent>())
        {
            UIMan->ToggleWindow(EGameWindowType::SystemMenu);
        }
    }
}