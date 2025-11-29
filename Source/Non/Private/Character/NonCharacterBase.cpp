#include "Character/NonCharacterBase.h"
#include "Ability/NonAbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Ability/GA_ComboBase.h"
#include "UI/QuickSlotManager.h"

#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"

#include "InputActionValue.h"
#include "Camera/CameraComponent.h"

#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Controller.h"

#include "DrawDebugHelpers.h"
#include "AI/EnemyCharacter.h"

#include "Effects/DamageNumberActor.h"
#include "Core/BPC_UIManager.h"
#include "Data/S_LevelData.h"
#include "Equipment/EquipmentComponent.h"
#include "Inventory/InventoryItem.h"

#include "Animation/AnimInstance.h"
#include "Animation/NonAnimInstance.h"     // ← AnimBP 경유
#include "Animation/AnimSetTypes.h"        // ← EWeaponStance 등
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"

#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"


ANonCharacterBase::ANonCharacterBase()
{
    PrimaryActorTick.bCanEverTick = true;

    // GAS
    AbilitySystemComponent = CreateDefaultSubobject<UNonAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UNonAttributeSet>(TEXT("AttributeSet"));

    // 카메라
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;

    UIManager = CreateDefaultSubobject<UBPC_UIManager>(TEXT("UIManager"));
    QuickSlotManager = CreateDefaultSubobject<UQuickSlotManager>(TEXT("QuickSlotManager"));
}

void ANonCharacterBase::BeginPlay()
{
    Super::BeginPlay();

    // ASC 캐싱
    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(this))
    {
        ASC = ASI->GetAbilitySystemComponent();
    }
    if (!ASC) ASC = FindComponentByClass<UAbilitySystemComponent>();
    if (!ASC)
    {
    }
    else
    {
    }

    // 장착/이동 초기화(네 기존 코드 유지)
    CachedArmedStance = DefaultArmedStance;
    SyncEquippedStanceFromEquipment();
    if (UCharacterMovementComponent* Move = GetCharacterMovement()) { WalkSpeed_Default = Move->MaxWalkSpeed; }
    RefreshWeaponStance();
    UpdateStrafeYawFollowBySpeed();

    // ── GAS Attribute 변경 델리게이트: UI 업데이트 + 사망 체크를 한 번에 처리 ──
    if (AbilitySystemComponent && AttributeSet)
    {
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHPAttribute())
            .AddLambda([this](const FOnAttributeChangeData& Data)
                {
                    // UI가 있을 때만 갱신 (플레이어 전용)
                    if (UIManager)
                    {
                        UIManager->UpdateHP(Data.NewValue, AttributeSet->GetMaxHP());
                    }

                    // HP 0 → 사망
                    if (!bDied && Data.OldValue > 0.f && Data.NewValue <= 0.f)
                    {
                        HandleDeath();
                    }
                });

        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetMPAttribute())
            .AddLambda([this](const FOnAttributeChangeData& Data)
                {
                    if (UIManager)
                    {
                        UIManager->UpdateMP(Data.NewValue, AttributeSet->GetMaxMP());
                    }
                });

        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetSPAttribute())
            .AddLambda([this](const FOnAttributeChangeData& Data)
                {
                    if (UIManager)
                    {
                        UIManager->UpdateSP(Data.NewValue, AttributeSet->GetMaxSP());
                    }
                });

        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetLevelAttribute())
            .AddLambda([this](const FOnAttributeChangeData& Data)
                {
                    if (UIManager)
                    {
                        UIManager->UpdateLevel(static_cast<int32>(Data.NewValue));
                    }
                });

        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetExpAttribute())
            .AddLambda([this](const FOnAttributeChangeData& Data)
                {
                    if (UIManager)
                    {
                        if (Data.NewValue >= AttributeSet->GetExpToNextLevel())
                        {
                            LevelUp();
                        }
                        UIManager->UpdateEXP(Data.NewValue, AttributeSet->GetExpToNextLevel());
                    }
                });
    }

    // ── 초기 UI 값 세팅 (플레이어 컨트롤일 때만) ──
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (AbilitySystemComponent && AttributeSet && UIManager)
        {
            UIManager->UpdateHP(AttributeSet->GetHP(), AttributeSet->GetMaxHP());
            UIManager->UpdateMP(AttributeSet->GetMP(), AttributeSet->GetMaxMP());
            UIManager->UpdateSP(AttributeSet->GetSP(), AttributeSet->GetMaxSP());
            UIManager->UpdateEXP(AttributeSet->GetExp(), AttributeSet->GetExpToNextLevel());
            UIManager->UpdateLevel(static_cast<int32>(AttributeSet->GetLevel()));
        }
    }

    if (!EquipmentComp) EquipmentComp = FindComponentByClass<UEquipmentComponent>();
    StartStaminaRegenLoop();
    if (!bDied && AttributeSet && AttributeSet->GetHP() <= 0.f) { HandleDeath(); }

    // === 스킬 매니저 초기화 ===
    if (!SkillMgr) SkillMgr = FindComponentByClass<USkillManagerComponent>();

    // 1) 서버/클라 공통: DataAsset / ASC 세팅 (UI에서 CanLevelUp 등에 사용)
    if (SkillMgr && ASC)
    {
        SkillMgr->Init(DefaultJobClass, nullptr, ASC);
    }

    // 2) 서버 전용: 직업/포인트 설정 (Replicated)
    if (HasAuthority() && SkillMgr)
    {
        SkillMgr->SetJobClass(DefaultJobClass);
        SkillMgr->AddSkillPoints(0);
    }
}

void ANonCharacterBase::ToggleArmed()
{
    SetArmed(!bArmed);
}


void ANonCharacterBase::SetEquippedStance(EWeaponStance NewStance)
{
    CachedArmedStance = NewStance;
}

void ANonCharacterBase::SetArmed(bool bNewArmed)
{
    if (bArmed == bNewArmed)
    {
        return;
    }

    // Draw 시점에 1H/2H/Staff 정확히 맞추도록, 장착 스탠스 먼저 동기화
    SyncEquippedStanceFromEquipment();

    //  Draw면 '장착 스탠스'에서 Equip을 고르고,
    //   Sheathe면 '현재 포즈 스탠스'에서 Unequip을 고른다.
    const EWeaponStance PrevStanceForMontage = bNewArmed ? GetEquippedStance() : GetWeaponStance();

    bArmed = bNewArmed;

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
        {
            if (UNonAnimInstance* NonAnim = Cast<UNonAnimInstance>(Anim))
            {
                // AnimBP 상태를 '몽타주 선택 기준 스탠스'로 동기화
                NonAnim->WeaponStance = PrevStanceForMontage;
                NonAnim->bArmed = bArmed;

                const FWeaponAnimSet& CurSet = NonAnim->GetWeaponAnimSet();
                UAnimMontage* Montage = bArmed ? CurSet.Equip : CurSet.Unequip;

                if (Montage)
                {
                    Anim->Montage_Play(Montage, 1.0f);

                    // 몽타주 종료 시 최종 스탠스만 정리 (태그는 AnimNotify에서 처리)
                    FOnMontageEnded End;
                    End.BindLambda([this, bTargetArmed = bArmed](UAnimMontage* M, bool bInterrupted)
                        {
                            // 1) 캐릭터 내부 스탠스 갱신
                            RefreshWeaponStance();

                            // 2) AnimInstance 로 즉시 반영
                            if (USkeletalMeshComponent* MeshComp2 = GetMesh())
                            {
                                if (UNonAnimInstance* NI = Cast<UNonAnimInstance>(MeshComp2->GetAnimInstance()))
                                {
                                    NI->bArmed = bArmed;        // 캐릭터 bArmed를 그대로 반영
                                    NI->WeaponStance = GetWeaponStance(); // 캐릭터 쪽 최신 스탠스를 반영
                                }
                            }
                        });
                    Anim->Montage_SetEndDelegate(End, Montage);
                }
                else
                {
                    // 실패 시 스탠스만 정리 (태그는 여전히 AnimNotify가 책임)
                    RefreshWeaponStance();
                }
            }
        }
    }
}

void ANonCharacterBase::MoveInput(const FInputActionValue& Value)
{
    // 기존 그대로
    FVector2D Movement = Value.Get<FVector2D>();
    if (Controller && Movement.SizeSquared() > 0.f)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        AddMovementInput(Forward, Movement.Y);
        AddMovementInput(Right, Movement.X);
    }
}

void ANonCharacterBase::SetStrafeMode(bool bEnable)
{
    bStrafeMode = bEnable;

    UCharacterMovementComponent* Move = GetCharacterMovement();
    if (!Move) return;

    if (!bStrafeMode)
    {
        bUseControllerRotationYaw = false;
        Move->bUseControllerDesiredRotation = false;
        Move->bOrientRotationToMovement = true;
    }
    else
    {
        Move->bOrientRotationToMovement = false;
    }

    UpdateStrafeYawFollowBySpeed();
}

void ANonCharacterBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 1) 공격 시작 정렬(카메라 방향으로 RInterpTo)
    UpdateAttackAlign(DeltaSeconds);

    // TIP 중/대기 중에는 자동 회전 모드 변경 차단
    if (!(bTIPPlaying || bTIP_Pending))
    {
        UpdateStrafeYawFollowBySpeed();
    }
    UpdateDirectionalSpeed();
    UpdateGuardDirAndSpeed();

    if (!bEnableTIP_Armed) return;

    UCharacterMovementComponent* Move = GetCharacterMovement();
    const float Speed2D = GetVelocity().Size2D();
    const bool  bAccelZero = Move ? Move->GetCurrentAcceleration().IsNearlyZero() : true;
    const bool  bOnGround = !(Move && Move->IsFalling());
    const bool  bIdle = (Speed2D < TIP_IdleSpeedEps) && bAccelZero && bOnGround;

    // 카메라-액터 각도
    const float CtrlYaw = GetControlRotation().Yaw;
    const float ActYaw = GetActorRotation().Yaw;
    AimYawDelta = FMath::FindDeltaAngleDegrees(ActYaw, CtrlYaw);
    const float AbsDelta = FMath::Abs(AimYawDelta);

    // 1) Idle 중 각도 조건 충족 시 '대기'만 설정 (즉시 재생 X)
    if (bIdle && (IsArmed() || bGuarding) && !IsAnyMontagePlaying())
    {
        if (GetWorld()->TimeSeconds >= TIP_NextAllowedTime)
        {
            if (AbsDelta >= TIP_Trigger180 || AbsDelta >= TIP_Trigger90)
            {
                bTIP_Pending = true;
                TIP_PendingDelta = AimYawDelta;  // 부호로 좌/우 결정
            }
            // 각도를 되돌렸으면 대기 취소(선택)
            else if (bTIP_Pending&& AbsDelta < 30.f)
            {
                bTIP_Pending = false;
                TIP_PendingDelta = 0.f;
            }
        }
    }

    // 2) 앞으로 '이동 시작(가속)' 감지되면 Pending 소비하며 즉시 재생
    if (bTIP_Pending && !IsAnyMontagePlaying() && (IsArmed() || bGuarding))
    {
        const FVector Accel = Move ? Move->GetCurrentAcceleration() : FVector::ZeroVector;
        const bool bAccelValid = Accel.SizeSquared() > KINDA_SMALL_NUMBER;

        if (bAccelValid)
        {
            const FRotator OnlyYaw(0.f, CtrlYaw, 0.f);
            const FVector  Forward = FRotationMatrix(OnlyYaw).GetUnitAxis(EAxis::X);
            const FVector  Accel2D = FVector(Accel.X, Accel.Y, 0.f).GetSafeNormal();

            const float ForwardDot = FVector::DotProduct(Accel2D, Forward); // +1: 정면
            const bool  bForwardStart = (ForwardDot >= TIP_ForwardDotMin);

            if (bForwardStart)
            {
                UAnimMontage* ToPlay = nullptr;
                const float AbsPending = FMath::Abs(TIP_PendingDelta);

                if (AbsPending >= TIP_Trigger180 && (TIP_L180 || TIP_R180))
                {
                    ToPlay = (TIP_PendingDelta >= 0.f) ? TIP_R180 : TIP_L180;
                }
                else if (AbsPending >= TIP_Trigger90 && (TIP_L90 || TIP_R90))
                {
                    ToPlay = (TIP_PendingDelta >= 0.f) ? TIP_R90 : TIP_L90;
                }

                if (ToPlay)
                {
                    // 회전 주도권 잠금: 이동/컨트롤이 Yaw 못 바꾸게
                    bUseControllerRotationYaw = false;
                    if (Move)
                    {
                        Move->bOrientRotationToMovement = false;
                        Move->bUseControllerDesiredRotation = false;
                    }

                    if (UAnimInstance* AI = (GetMesh() ? GetMesh()->GetAnimInstance() : nullptr))
                    {
                        AI->Montage_Play(ToPlay, 1.f);

                        bTIPPlaying = true;
                        FOnMontageEnded End;
                        End.BindUObject(this, &ANonCharacterBase::OnTIPMontageEnded);
                        AI->Montage_SetEndDelegate(End, ToPlay);

                        TIP_NextAllowedTime = GetWorld()->TimeSeconds + TIP_Cooldown;
                    }
                }

                // 소비 후 리셋
                bTIP_Pending = false;
                TIP_PendingDelta = 0.f;
            }
        }
    }
}

void ANonCharacterBase::OnTIPMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    bTIPPlaying = false;

    // TIP 끝난 뒤 정렬(팝 최소화)
    const float CtrlYaw = GetControlRotation().Yaw;
    SetActorRotation(FRotator(0.f, CtrlYaw, 0.f));

    // 여기서 '카메라 따라 회전'을 켜지 말 것
    bUseControllerRotationYaw = false;
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->bUseControllerDesiredRotation = false;
        Move->bOrientRotationToMovement = false; // 무장 Idle에서 제자리 유지
    }
}


void ANonCharacterBase::UpdateDirectionalSpeed()
{
    UCharacterMovementComponent* Move = GetCharacterMovement();
    if (!Move) return;

    auto RestoreDefault = [&]()
        {
            if (WalkSpeed_Default > 0.f)
                Move->MaxWalkSpeed = WalkSpeed_Default;
        };

    if (!bStrafeMode)
    {
        RestoreDefault();
        return;
    }

    const FVector2D Input2D = FVector2D(GetLastMovementInputVector().X, GetLastMovementInputVector().Y);
    const bool bHasInput = Input2D.SizeSquared() > KINDA_SMALL_NUMBER;

    if (!bHasInput || Move->IsFalling())
    {
        RestoreDefault();
        return;
    }

    const FVector ControlFwd = FRotationMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::X);
    const FVector Input3D = FVector(Input2D.X, Input2D.Y, 0.f).GetSafeNormal();
    const float Dot = FVector::DotProduct(Input3D, ControlFwd);

    const float BackDotThreshold = FMath::Cos(FMath::DegreesToRadians(BackpedalDirThresholdDeg));
    const bool bBackInput = (Dot <= BackDotThreshold);

    if (bBackInput)
    {
        Move->MaxWalkSpeed = WalkSpeed_Backpedal;

        const float Speed2D = Move->Velocity.Size2D();
        if (Speed2D > WalkSpeed_Backpedal + 1.f)
        {
            const FVector NewVel = FVector(Move->Velocity.X, Move->Velocity.Y, 0.f).GetSafeNormal() * WalkSpeed_Backpedal;
            Move->Velocity = FVector(NewVel.X, NewVel.Y, Move->Velocity.Z);
        }
    }
    else
    {
        RestoreDefault();
    }
}

void ANonCharacterBase::UpdateStrafeYawFollowBySpeed()
{
    UCharacterMovementComponent* Move = GetCharacterMovement();
    if (!Move) return;

    if (bTIPPlaying || bTIP_Pending) return;

    // === 0) 태그 읽기 ===
    bool bHasLockTag = false;
    bool bHasAttackLikeTag = false;

    if (AbilitySystemComponent)
    {
        static const FGameplayTag DodgeTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dodge"));
        static const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attack"));
        static const FGameplayTag ComboTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Active.Combo"));

        const bool bDodge = AbilitySystemComponent->HasMatchingGameplayTag(DodgeTag);
        const bool bAttack = AbilitySystemComponent->HasMatchingGameplayTag(AttackTag);
        const bool bCombo = AbilitySystemComponent->HasMatchingGameplayTag(ComboTag);

        bHasLockTag = (bDodge || bAttack || bCombo);
        bHasAttackLikeTag = (bAttack || bCombo);
    }

    // === 1) 회피/공격/콤보 중: 회전 잠금 + 공격 시작 정렬 ===
    if (bHasLockTag)
    {
        const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;

        // 잠금 상태 진입(첫 프레임)
        if (!bRotationLockedByAbility)
        {
            bRotationLockedByAbility = true;
            bAlignYawAfterRotationLock = false;
        }

        // 잠금 중에는 자동 회전 끔
        bFollowCameraYawNow = false;
        bUseControllerRotationYaw = false;
        Move->bUseControllerDesiredRotation = false;
        Move->bOrientRotationToMovement = false;
        Move->RotationRate = FRotator(0.f, 0.f, 0.f);
        return;
    }

    // === 2) 태그가 사라진 직후: 끝난 후 정렬 ===
    if (bRotationLockedByAbility)
    {
        bRotationLockedByAbility = false;
        bAlignYawAfterRotationLock = true;
    }

    if (bAlignYawAfterRotationLock)
    {
        const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;

        const FRotator Current = GetActorRotation();
        FRotator       Target = GetControlRotation();
        Target.Pitch = Current.Pitch;
        Target.Roll = Current.Roll;

        // 여기도 필요하면 ConstantTo로 바꿔도 됨
        const FRotator NewRot =
            FMath::RInterpTo(Current, Target, DeltaSeconds, RotationAlignInterpSpeed);
        SetActorRotation(NewRot);

        const float DeltaYaw =
            FMath::Abs(FMath::FindDeltaAngleDegrees(NewRot.Yaw, Target.Yaw));
        if (DeltaYaw < 1.f)
        {
            bAlignYawAfterRotationLock = false;
        }

        bFollowCameraYawNow = false;
        bUseControllerRotationYaw = false;
        Move->bUseControllerDesiredRotation = false;
        Move->bOrientRotationToMovement = false;
        Move->RotationRate = FRotator(0.f, 0.f, 0.f);
        return;
    }

    // === 3) 평상시 스트레이프/비스트레이프 ===

    if (!bStrafeMode)
    {
        bFollowCameraYawNow = false;
        bUseControllerRotationYaw = false;
        Move->bUseControllerDesiredRotation = false;
        Move->bOrientRotationToMovement = true;
        Move->RotationRate = FRotator(0.f, 540.f, 0.f);
        return;
    }

    const float Speed2D = GetVelocity().Size2D();
    const bool  bMoving = (Speed2D >= FMath::Max(TIP_IdleSpeedEps, YawFollowEnableSpeed)) && !Move->IsFalling();
    const bool  bShouldFollowCamera = bOnlyFollowCameraWhenMoving ? bMoving : true;

    if (bFollowCameraYawNow != bShouldFollowCamera)
    {
        bFollowCameraYawNow = bShouldFollowCamera;

        if (bShouldFollowCamera)
        {
            bUseControllerRotationYaw = true;
            Move->bUseControllerDesiredRotation = true;
            Move->bOrientRotationToMovement = false;
            Move->RotationRate = FRotator(0.f, 720.f, 0.f);
        }
        else
        {
            bUseControllerRotationYaw = false;
            Move->bUseControllerDesiredRotation = false;
            Move->bOrientRotationToMovement = false;
            Move->RotationRate = FRotator(0.f, 540.f, 0.f);
        }
    }
}

void ANonCharacterBase::LookInput(const FInputActionValue& Value)
{
    FVector2D LookAxis = Value.Get<FVector2D>();
    AddControllerYawInput(LookAxis.X);
    AddControllerPitchInput(LookAxis.Y);
}

void ANonCharacterBase::HandleAttackInput(const FInputActionValue& Value)
{
    if (IsComboWindowOpen())
    {
        BufferComboInput();
    }
    else
    {
        TryActivateCombo();
    }
}

void ANonCharacterBase::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);

        if (HasAuthority())
        {
            InitializeAttributes();
            GiveStartupAbilities();
        }
    }
}

UAbilitySystemComponent* ANonCharacterBase::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void ANonCharacterBase::InitializeAttributes()
{
    if (AbilitySystemComponent && DefaultAttributeEffect)
    {
        FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
        EffectContext.AddSourceObject(this);

        FGameplayEffectSpecHandle NewHandle = AbilitySystemComponent->MakeOutgoingSpec(DefaultAttributeEffect, 1.0f, EffectContext);
        if (NewHandle.IsValid())
        {
            AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*NewHandle.Data.Get(), AbilitySystemComponent);
        }
    }
}

void ANonCharacterBase::GiveStartupAbilities()
{
    if (!HasAuthority() || !AbilitySystemComponent)
    {
        return;
    }

    // 1) 공통 GA 먼저 부여 (모든 직업 공통으로 쓰는 것들)
    for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
    {
        if (AbilityClass)
        {
            AbilitySystemComponent->GiveAbility(
                FGameplayAbilitySpec(
                    AbilityClass,
                    1,
                    static_cast<int32>(AbilitySystemComponent->GetActivatableAbilities().Num()),
                    this));
        }
    }

    // 2) 직업별 GA 세트 적용
    const EJobClass JobToUse = DefaultJobClass; // 지금은 DefaultJobClass 기준

    if (const FJobAbilitySet* FoundSet = JobAbilitySets.Find(JobToUse))
    {
        for (const TSubclassOf<UGameplayAbility>& AbilityClass : FoundSet->Abilities)
        {
            if (AbilityClass)
            {
                AbilitySystemComponent->GiveAbility(
                    FGameplayAbilitySpec(
                        AbilityClass,
                        1,
                        static_cast<int32>(AbilitySystemComponent->GetActivatableAbilities().Num()),
                        this));
            }
        }
    }
}


void ANonCharacterBase::SetComboWindowOpen(bool bOpen)
{
    bComboWindowOpen = bOpen;

    if (CurrentComboAbility.IsValid())
    {
        CurrentComboAbility->SetComboWindowOpen(bOpen);
    }
}

void ANonCharacterBase::BufferComboInput()
{
    if (CurrentComboAbility.IsValid())
    {
        CurrentComboAbility->BufferComboInput();
    }
}

bool ANonCharacterBase::IsComboWindowOpen() const
{
    return bComboWindowOpen;
}

void ANonCharacterBase::TryActivateCombo()
{
    if (!AbilitySystemComponent)
    {
        return;
    }

    // 이미 콤보 어빌리티(Ability.Active.Combo)가 돌고 있으면 새로 시작하지 않음
    const FGameplayTag ActiveComboTag = FGameplayTag::RequestGameplayTag(FName("Ability.Active.Combo"));
    if (AbilitySystemComponent->HasMatchingGameplayTag(ActiveComboTag))
    {
        return;
    }

    // Ability.Combo1 태그를 가진 GA를 찾아서 실행
    const FGameplayTag Combo1Tag = FGameplayTag::RequestGameplayTag(FName("Ability.Combo1"));

    FGameplayTagContainer TagContainer;
    TagContainer.AddTag(Combo1Tag);

    const bool bSuccess = AbilitySystemComponent->TryActivateAbilitiesByTag(TagContainer);

#if WITH_EDITOR
    UE_LOG(LogTemp, Log,
        TEXT("[Combo] TryActivateAbilitiesByTag(Ability.Combo1) => %s"),
        bSuccess ? TEXT("Success") : TEXT("Fail"));
#endif
}

// 등록/해제
void ANonCharacterBase::RegisterCurrentComboAbility(UGA_ComboBase* InAbility)
{
    CurrentComboAbility = InAbility;
}
void ANonCharacterBase::UnregisterCurrentComboAbility(UGA_ComboBase* InAbility)
{
    if (CurrentComboAbility.Get() == InAbility)
    {
        CurrentComboAbility = nullptr;
    }
}

void ANonCharacterBase::LevelUp()
{
    if (!LevelDataTable || !AttributeSet || !AbilitySystemComponent)
        return;

    int32 CurrentLevel = AttributeSet->GetLevel();
    int32 NewLevel = CurrentLevel + 1;

    FLevelData* NewData = LevelDataTable->FindRow<FLevelData>(*FString::FromInt(NewLevel), TEXT(""));
    if (!NewData)
    {
        return;
    }

    FGameplayEffectContextHandle Context;
    FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(GE_LevelUp, 1.f, Context);
    if (Spec.IsValid())
    {
        Spec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.MaxHP")), NewData->MaxHP);
        Spec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.MaxMP")), NewData->MaxMP);
        Spec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.ExpToNextLevel")), NewData->ExpToNextLevel);

        AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetLevelAttribute(), NewLevel);
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetHPAttribute(), NewData->MaxHP);
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetMPAttribute(), NewData->MaxMP);

        float CurrentStat = AttributeSet->GetStatPoint();
        float CurrentSkill = AttributeSet->GetSkillPoint();
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetStatPointAttribute(), CurrentStat + 5.f);
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetSkillPointAttribute(), CurrentSkill + 1.f);

        UIManager->UpdateHP(NewData->MaxHP, NewData->MaxHP);
        UIManager->UpdateMP(NewData->MaxMP, NewData->MaxMP);
        UIManager->UpdateEXP(AttributeSet->GetExp(), NewData->ExpToNextLevel);
        UIManager->UpdateLevel(NewLevel);
    }
}

void ANonCharacterBase::GainExp(float Amount)
{
    if (AbilitySystemComponent && AttributeSet)
    {
        float CurrentExp = AttributeSet->GetExp();
        float MaxExp = AttributeSet->GetExpToNextLevel();
        float NewExp = CurrentExp + Amount;
        for (int i = 0; i < 10; ++i)
        {
            if (NewExp < MaxExp)
                break;

            NewExp -= MaxExp;
            LevelUp();
            MaxExp = AttributeSet->GetExpToNextLevel();
            if (MaxExp <= 0.f)
                break;
        }

        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetExpAttribute(), NewExp);

        if (UIManager)
        {
            UIManager->UpdateEXP(NewExp, AttributeSet->GetExpToNextLevel());
            UIManager->UpdateLevel(AttributeSet->GetLevel());
        }
    }
}

FName ANonCharacterBase::GetHandSocketForItem(const UInventoryItem* Item) const
{
    if (!Item) return HandSocket1H;
    const bool bTwoHand = Item->IsTwoHandedWeapon();
    return bTwoHand ? HandSocket2H : HandSocket1H;
}

void ANonCharacterBase::RefreshWeaponStance()
{
    UEquipmentComponent* Eq = FindComponentByClass<UEquipmentComponent>();
    UInventoryItem* Main = Eq ? Eq->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain) : nullptr;

    if (!bArmed || !Main)
    {
        WeaponStance = EWeaponStance::Unarmed;
        SetStrafeMode(false);
        return;
    }

    const bool bIsStaff = (Main->CachedRow.WeaponType == EWeaponType::Staff);
    const bool bIsTwoHand = Main->IsTwoHandedWeapon() || bIsStaff;

    EWeaponStance NewStance =
        bIsStaff ? EWeaponStance::Staff :
        bIsTwoHand ? EWeaponStance::TwoHanded :
        EWeaponStance::OneHanded;

    if (WeaponStance != NewStance)
    {
        WeaponStance = NewStance;
    }

    SetStrafeMode(true);
}

FName ANonCharacterBase::GetHomeFallbackSocketForItem(const UInventoryItem* Item) const
{
    const bool bTwoHand = (Item && Item->IsTwoHandedWeapon());
    return bTwoHand ? SheathSocket2H : SheathSocket1H;
}

static EGuardDir8 Quantize8(float AngleDeg)
{
    float A = FMath::Fmod(AngleDeg + 360.f, 360.f);
    int32 idx = FMath::FloorToInt((A + 22.5f) / 45.f) % 8;
    return static_cast<EGuardDir8>(idx);
}

void ANonCharacterBase::GuardPressed()
{
    if (!IsArmed())
        return;

    if (CurrentComboAbility.IsValid())
    {
        const FGameplayTag Tag_Combo3 = FGameplayTag::RequestGameplayTag(FName("Ability.Combo3"));
        if (CurrentComboAbility->GetAssetTags().HasTagExact(Tag_Combo3))
        {
            return;
        }
    }

    StartGuard();
}

void ANonCharacterBase::GuardReleased()
{
    StopGuard();
}

void ANonCharacterBase::StartGuard()
{
    if (bGuarding) return;
    if (!IsArmed()) return;

    bGuarding = true;

    if (USkeletalMeshComponent* SkelComp = GetMesh())
    {
        if (UAnimInstance* Anim = SkelComp->GetAnimInstance())
        {
            Anim->Montage_StopGroupByName(0.1f, FName("DefaultGroup"));
        }
    }

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->MaxWalkSpeed = GuardWalkSpeed;
        bUseControllerRotationYaw = true;
        Move->bOrientRotationToMovement = false;
        Move->bUseControllerDesiredRotation = true;
        Move->RotationRate = FRotator(0.f, 720.f, 0.f);
    }

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Guard")));

        const FGameplayTagContainer WithTags(FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack")));
        AbilitySystemComponent->CancelAbilities(&WithTags, nullptr, nullptr);
    }
}

void ANonCharacterBase::StopGuard()
{
    if (!bGuarding) return;
    bGuarding = false;

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Guard")));
    }

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        if (WalkSpeed_Default > 0.f)
            Move->MaxWalkSpeed = WalkSpeed_Default;
    }

    RefreshWeaponStance();
}

void ANonCharacterBase::UpdateGuardDirAndSpeed()
{
    if (!bGuarding) return;

    UCharacterMovementComponent* Move = GetCharacterMovement();
    if (!Move) return;

    const float BaseYaw = GetControlRotation().Yaw;

    const FVector2D In = FVector2D(GetLastMovementInputVector().X, GetLastMovementInputVector().Y);
    const bool bHasInput = In.SizeSquared() > KINDA_SMALL_NUMBER;

    if (!bHasInput)
    {
        GuardDirAngle = 0.f;
        GuardDir8 = EGuardDir8::F;
    }
    else
    {
        const float InYaw = FMath::RadiansToDegrees(FMath::Atan2(In.Y, In.X));
        const float Rel = FMath::FindDeltaAngleDegrees(InYaw, BaseYaw);
        GuardDirAngle = Rel;
        GuardDir8 = Quantize8(Rel);
    }

    const float AbsRel = FMath::Abs(GuardDirAngle);
    static bool bBackZone = false;
    const float Enter = GuardBackEnterDeg;
    const float Exit = GuardBackExitDeg;

    if (!bBackZone && AbsRel >= Enter)
        bBackZone = true;
    else if (bBackZone && AbsRel <= Exit)
        bBackZone = false;

    if (bBackZone)
    {
        Move->MaxWalkSpeed = GuardBackSpeed;
        const float Spd = Move->Velocity.Size2D();
        if (Spd > GuardBackSpeed + 1.f)
        {
            const FVector NewXY = FVector(Move->Velocity.X, Move->Velocity.Y, 0.f).GetSafeNormal() * GuardBackSpeed;
            Move->Velocity = FVector(NewXY.X, NewXY.Y, Move->Velocity.Z);
        }
    }
    else
    {
        Move->MaxWalkSpeed = GuardWalkSpeed;
    }
}

void ANonCharacterBase::ApplyDamageAt(float Amount, AActor* DamageInstigator, const FVector& WorldLocation)
{
    // ── 0) I-Frame이 우선 ─────────────────────────────────────────
    if (AbilitySystemComponent)
    {
        static const FGameplayTag Tag_IFrame = FGameplayTag::RequestGameplayTag(TEXT("State.IFrame"));
        const bool bIFrame = AbilitySystemComponent->HasMatchingGameplayTag(Tag_IFrame);

        // 월드에 표시(작게)
        if (UWorld* W = GetWorld())
        {
            DrawDebugSphere(W, WorldLocation, 8.f, 8, bIFrame ? FColor::Cyan : FColor::Red, false, 2.0f);
        }

        if (bIFrame)
        {
            if (HasAuthority())
            {
                Multicast_SpawnDodgeText(WorldLocation);
            }
            return; // I-Frame이면 여기서 끝!
        }
    }

    // 0 이하 무시
    if (Amount <= 0.f) return;

    // ── 1) 정상 데미지(GAS 우선) ───────────────────────────────────
    if (AbilitySystemComponent && GE_Damage)
    {
        FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
        Ctx.AddInstigator(DamageInstigator, Cast<APawn>(DamageInstigator) ? Cast<APawn>(DamageInstigator)->GetController() : nullptr);

        FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(GE_Damage, 1.f, Ctx);
        if (Spec.IsValid())
        {
            static const FGameplayTag Tag_DataDamage = FGameplayTag::RequestGameplayTag(TEXT("Data.Damage"));
            Spec.Data->SetSetByCallerMagnitude(Tag_DataDamage, -Amount);
            AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        }
    }
    else if (AbilitySystemComponent && AttributeSet)
    {
        const float Cur = AttributeSet->GetHP();
        const float Max = AttributeSet->GetMaxHP();
        const float NewVal = FMath::Clamp(Cur - Amount, 0.f, Max);
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetHPAttribute(), NewVal);
    }
    else
    {
        // (선택) 완전 폴백
        UGameplayStatics::ApplyPointDamage(
            this, Amount, FVector::UpVector, FHitResult(WorldLocation, FVector::UpVector),
            DamageInstigator ? DamageInstigator->GetInstigatorController() : nullptr,
            DamageInstigator ? DamageInstigator : this,
            UDamageType::StaticClass()
        );
    }

    // ── 2) 숫자 플로팅은 여기서만 ─────────────────────────────────
    if (HasAuthority())
    {
        Multicast_SpawnDamageNumber(Amount, WorldLocation, /*bCritical=*/false);
    }

    // ── 3) 히트 리액션(히트스톱/넉백 등) ─────────────────────────
    OnGotHit(Amount, DamageInstigator, WorldLocation);
}

void ANonCharacterBase::Multicast_SpawnDamageNumber_Implementation(float Amount, FVector WorldLocation, bool bIsCritical)
{
    if (Amount <= 0.f) return; // 0이면 아무 것도 표시하지 않음 (안전장치)

    if (!GetWorld()) return;

    // 폴백: 충돌 좌표가 0,0,0 이면 머리/가슴쯤에서 뜨게 보정
    if (WorldLocation.IsNearlyZero())
    {
        if (USkeletalMeshComponent* Skel = GetMesh())
        {
            // spine이나 head 소켓이 있으면 우선 사용
            const FName PrefSockets[] = { TEXT("head"), TEXT("spine_03"), TEXT("spine_02"), TEXT("spine_01") };
            bool bDone = false;
            for (const FName& S : PrefSockets)
            {
                if (Skel->DoesSocketExist(S))
                {
                    WorldLocation = Skel->GetSocketLocation(S) + FVector(0, 0, 20.f);
                    bDone = true; break;
                }
            }
            if (!bDone)
            {
                const FBoxSphereBounds B = Skel->Bounds;
                WorldLocation = B.Origin + FVector(0, 0, B.BoxExtent.Z * 0.8f);
            }
        }
        else
        {
            WorldLocation = GetActorLocation() + FVector(0, 0, 100.f);
        }
    }

    TSubclassOf<ADamageNumberActor> SpawnClass = DamageNumberClass;
    if (!SpawnClass) SpawnClass = ADamageNumberActor::StaticClass();

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SP.Owner = this;

    if (ADamageNumberActor* A = GetWorld()->SpawnActor<ADamageNumberActor>(SpawnClass, WorldLocation, FRotator::ZeroRotator, SP))
    {
        A->InitWithFlags(Amount, bIsCritical);
    }
}

void ANonCharacterBase::Multicast_SpawnDodgeText_Implementation(FVector WorldLocation)
{
    if (!DamageNumberClass) return;
    UWorld* W = GetWorld(); if (!W) return;

    const FActorSpawnParameters P = [] {
        FActorSpawnParameters S;
        S.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return S;
        }();

    ADamageNumberActor* A = W->SpawnActor<ADamageNumberActor>(DamageNumberClass, WorldLocation, FRotator::ZeroRotator, P);
    if (A)
    {
        A->SetupAsDodge();
        A->SetOwner(this);
    }
}

void ANonCharacterBase::OnGotHit(float Damage, AActor* InstigatorActor, const FVector& ImpactPoint)
{
    if (IsDead() || Damage <= 0.f) return;

    // 히트스톱
    ApplyHitStopSelf(0.25f, 0.07f);

    // 넉백 모드 처리(기존 그대로)
    if (KnockbackMode == EKnockbackMode::Launch)
    {
        const FVector Dir = ComputeKnockbackDir(InstigatorActor, ImpactPoint);
        FVector Impulse = Dir * LaunchStrength;
        const bool bUseZ = FMath::Abs(LaunchUpward) > KINDA_SMALL_NUMBER;
        if (bUseZ)
        {
            Impulse.Z += LaunchUpward;
            LaunchCharacter(Impulse, /*bXYOverride=*/true, /*bZOverride=*/true);
        }
        else
        {
            LaunchCharacter(Impulse, /*bXYOverride=*/true, /*bZOverride=*/false);
        }
    }

    PlayHitReact(ComputeHitQuadrant(ImpactPoint, InstigatorActor));
}

void ANonCharacterBase::PlayHitReact(EHitQuadrant /*Quad*/)
{
    if (!GetMesh()) return;

    if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
    {
        if (UNonAnimInstance* NonAnim = Cast<UNonAnimInstance>(Anim))
        {
            // 무기별 우선 → 공통 폴백
            if (UAnimMontage* M = NonAnim->GetHitReact_Light())
            {
                Anim->Montage_Play(M, 1.0f);
                return;
            }
        }

        // 최종 폴백 없음(예전 방향별은 제거됨)
    }
}

FVector ANonCharacterBase::ComputeKnockbackDir(AActor* InstigatorActor, const FVector& ImpactPoint) const
{
    // 1) 충돌 지점이 유효하면: 충돌 지점 → 내 위치 방향(수평)
    if (!ImpactPoint.IsNearlyZero())
    {
        FVector D = (GetActorLocation() - ImpactPoint);
        D.Z = 0.f;
        if (!D.IsNearlyZero()) return D.GetSafeNormal();
    }

    // 2) 가해자 액터가 있으면: 가해자 → 내 위치(수평)
    if (InstigatorActor)
    {
        FVector D = (GetActorLocation() - InstigatorActor->GetActorLocation());
        D.Z = 0.f;
        if (!D.IsNearlyZero()) return D.GetSafeNormal();
    }

    // 3) 가해자 Pawn의 전방 반대 방향(가해자 전방 기준으로 내가 밀려남)
    if (const APawn* P = Cast<APawn>(InstigatorActor))
    {
        FVector D = -P->GetActorForwardVector();
        D.Z = 0.f;
        if (!D.IsNearlyZero()) return D.GetSafeNormal();
    }

    // 4) 마지막 폴백: 내 전방 반대
    FVector D = -GetActorForwardVector();
    D.Z = 0.f;
    return D.GetSafeNormal();
}

EHitQuadrant ANonCharacterBase::ComputeHitQuadrant(const FVector& ImpactPoint, AActor* InstigatorActor) const
{
    auto ToQuadrant = [&](const FVector& From, const FVector& To)
        {
            FVector Fwd = GetActorForwardVector(); Fwd.Z = 0.f; Fwd.Normalize();
            FVector Dir = (To - From); Dir.Z = 0.f; Dir.Normalize();

            if (Dir.IsNearlyZero()) return EHitQuadrant::Front;

            const float Dot = FVector::DotProduct(Fwd, Dir);
            const float CrossZ = FVector::CrossProduct(Fwd, Dir).Z;

            const bool bFrontHalf = (Dot >= 0.f);
            if (bFrontHalf)
            {
                return (CrossZ >= 0.f) ? EHitQuadrant::Right : EHitQuadrant::Left;
            }
            else
            {
                return EHitQuadrant::Back;
            }
        };

    // 1) ImpactPoint가 유효하면: ImpactPoint 기준
    if (!ImpactPoint.IsNearlyZero())
    {
        return ToQuadrant(GetActorLocation(), ImpactPoint);
    }

    // 2) 가해자 위치가 있으면: Instigator 기준
    if (InstigatorActor)
    {
        return ToQuadrant(GetActorLocation(), InstigatorActor->GetActorLocation());
    }

    // 3) 가해자 전방 반대(추정)
    if (const APawn* P = Cast<APawn>(InstigatorActor))
    {
        FVector FakeImpact = GetActorLocation() + (-P->GetActorForwardVector()) * 100.f;
        return ToQuadrant(GetActorLocation(), FakeImpact);
    }

    // 4) 폴백: 정면
    return EHitQuadrant::Front;
}

void ANonCharacterBase::ApplyHitStopSelf(float Scale, float Duration)
{
    Scale = FMath::Clamp(Scale, 0.05f, 1.f);
    CustomTimeDilation = Scale;

    GetWorldTimerManager().ClearTimer(HitStopTimer);
    GetWorldTimerManager().SetTimer(HitStopTimer, [this]()
        {
            CustomTimeDilation = 1.f;
        }, Duration, false);
}

bool ANonCharacterBase::HasZeroHP() const
{
    return (AttributeSet && AttributeSet->GetHP() <= 0.f);
}

bool ANonCharacterBase::IsDead() const
{
    return bDied || HasZeroHP();
}

void ANonCharacterBase::HandleDeath()
{
    if (bDied) return;
    bDied = true;

    // 이동/입력 차단
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->DisableMovement();
        Move->StopMovementImmediately();
    }
    if (AController* C = GetController())
    {
        C->SetIgnoreMoveInput(true);
        C->SetIgnoreLookInput(true);
    }
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 죽음 몽타주가 있으면 재생 (AnimBP 우선)
    if (bUseDeathMontage && GetMesh())
    {
        if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
        {
            if (class UNonAnimInstance* NonAnim = Cast<UNonAnimInstance>(Anim))
            {
                if (UAnimMontage* M = NonAnim->GetDeathMontage())
                {
                    const float Len = Anim->Montage_Play(M, 1.0f);
                    SetLifeSpan(FMath::Max(1.0f, Len + 0.25f));
                    return;
                }
            }

            // 폴백: (Deprecated) 기존 DeathMontage
            if (DeathMontage)
            {
                const float Len = Anim->Montage_Play(DeathMontage, 1.0f);
                SetLifeSpan(FMath::Max(1.0f, Len + 0.25f));
                return;
            }
        }
    }

    // 폴백: 래그돌
    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        Skel->SetCollisionProfileName(TEXT("Ragdoll"));
        Skel->SetSimulatePhysics(true);
        Skel->WakeAllRigidBodies();
    }
    SetLifeSpan(5.f);
}
// ===== Stamina =====

bool ANonCharacterBase::HasEnoughStamina(float Cost) const
{
    const float CurSP = (AttributeSet ? AttributeSet->GetSP() : 0.f);
    const float Needed = FMath::Max(0.f, Cost);
    return CurSP >= Needed;
}

bool ANonCharacterBase::ConsumeStamina(float Amount)
{
    if (Amount <= 0.f || !AbilitySystemComponent || !AttributeSet)
        return false;

    if (!HasEnoughStamina(Amount))
    {
        return false;
    }

    if (GE_StaminaDeltaInstant)
    {
        FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
        FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(GE_StaminaDeltaInstant, 1.f, Ctx);
        if (Spec.IsValid())
        {
            const FGameplayTag Tag_SPDelta = FGameplayTag::RequestGameplayTag(FName("Data.SPDelta"));
            Spec.Data->SetSetByCallerMagnitude(Tag_SPDelta, -Amount);
            AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        }
    }
    else
    {
        ApplyStaminaDelta_Direct(-Amount);
    }

    KickStaminaRegenDelay();
    return true;
}

void ANonCharacterBase::ApplyStaminaDelta_Direct(float Delta)
{
    if (!AbilitySystemComponent || !AttributeSet) return;

    const FGameplayAttribute AttrSP = UNonAttributeSet::GetSPAttribute();
    const float Cur = AttributeSet->GetSP();
    const float Max = AttributeSet->GetMaxSP();
    const float NewVal = FMath::Clamp(Cur + Delta, 0.f, Max);

    AbilitySystemComponent->SetNumericAttributeBase(AttrSP, NewVal);
}

void ANonCharacterBase::StartStaminaRegenLoop()
{
    if (!bAllowStaminaRegen) return;
    if (StaminaRegenTickInterval <= 0.f || StaminaRegenTickAmount <= 0.f) return;
    if (GetWorldTimerManager().IsTimerActive(StaminaRegenLoopTimer)) return;

    // 현재가 이미 Max면 시작할 필요 없음
    if (AttributeSet && AttributeSet->GetSP() >= AttributeSet->GetMaxSP() - KINDA_SMALL_NUMBER)
        return;

    GetWorldTimerManager().SetTimer(
        StaminaRegenLoopTimer,
        this,
        &ANonCharacterBase::StaminaRegenTick,
        StaminaRegenTickInterval,
        true   // looping
    );
}

void ANonCharacterBase::StopStaminaRegenLoop()
{
    GetWorldTimerManager().ClearTimer(StaminaRegenLoopTimer);
}

void ANonCharacterBase::StaminaRegenTick()
{
    if (!bAllowStaminaRegen || !AbilitySystemComponent || !AttributeSet)
        return;

    const float Cur = AttributeSet->GetSP();
    const float Max = AttributeSet->GetMaxSP();

    if (Cur >= Max - KINDA_SMALL_NUMBER)
    {
        // 이미 가득 찼으면 루프 중지
        StopStaminaRegenLoop();
        return;
    }

    // 한 번에 고정량 회복
    ApplyStaminaDelta_Direct(+StaminaRegenTickAmount);

    // 채웠다면 루프 중지
    const float NewCur = AttributeSet->GetSP();
    if (NewCur >= Max - KINDA_SMALL_NUMBER)
    {
        StopStaminaRegenLoop();
    }
}

void ANonCharacterBase::KickStaminaRegenDelay()
{
    bAllowStaminaRegen = false;

    // 지연 중에는 루프 정지
    StopStaminaRegenLoop();

    if (StaminaRegenDelayAfterUse <= 0.f)
    {
        bAllowStaminaRegen = true;
        StartStaminaRegenLoop();   // 바로 재개
        return;
    }

    GetWorldTimerManager().ClearTimer(StaminaRegenTimer);
    GetWorldTimerManager().SetTimer(
        StaminaRegenTimer,
        [this]()
        {
            bAllowStaminaRegen = true;
            StartStaminaRegenLoop();  // 지연 끝 → 루프 재개
        },
        StaminaRegenDelayAfterUse,
        false
    );
}

EWeaponStance ANonCharacterBase::ComputeStanceFromItem(const UInventoryItem* Item) const
{
    if (!Item) return EWeaponStance::Unarmed;

    const bool bIsStaff = (Item->CachedRow.WeaponType == EWeaponType::Staff);
    const bool bTwoHand = Item->IsTwoHandedWeapon() || bIsStaff;

    if (bIsStaff)   return EWeaponStance::Staff;
    if (bTwoHand)   return EWeaponStance::TwoHanded;
    return EWeaponStance::OneHanded;
}

void ANonCharacterBase::SyncEquippedStanceFromEquipment()
{
    UInventoryItem* Main = nullptr;
    if (!EquipmentComp)
    {
        EquipmentComp = FindComponentByClass<UEquipmentComponent>();
    }
    if (EquipmentComp)
    {
        Main = EquipmentComp->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain);
    }

    // 메인 무기가 있으면 그 무기 기준으로, 없으면 기본값 유지
    const EWeaponStance NewEquipped = ComputeStanceFromItem(Main);
    if (NewEquipped != EWeaponStance::Unarmed)
    {
        if (CachedArmedStance != NewEquipped)
        {
            CachedArmedStance = NewEquipped;
        }
    }
}

bool ANonCharacterBase::IsAnyMontagePlaying() const
{
    if (const USkeletalMeshComponent* MeshComp = GetMesh())
        if (const UAnimInstance* AI = MeshComp->GetAnimInstance())
            return AI->IsAnyMontagePlaying();
    return false;
}

void ANonCharacterBase::TryDodge()
{
    if (!AbilitySystemComponent)
    {
        return;
    }

    // 이미 Dodge 상태면 또 발동 안 함 (선택사항)
    static const FGameplayTag DodgeStateTag =
        FGameplayTag::RequestGameplayTag(TEXT("State.Dodge"));
    if (AbilitySystemComponent->HasMatchingGameplayTag(DodgeStateTag))
    {
        return;
    }

    // Ability.Dodge 태그 가진 GA들 발동
    static const FGameplayTag DodgeAbilityTag =
        FGameplayTag::RequestGameplayTag(TEXT("Ability.Dodge"));

    FGameplayTagContainer DodgeTagContainer;   // ← 이름을 Tags가 아니라 이렇게
    DodgeTagContainer.AddTag(DodgeAbilityTag);

    const bool bSuccess = AbilitySystemComponent->TryActivateAbilitiesByTag(DodgeTagContainer);

#if WITH_EDITOR
    UE_LOG(LogTemp, Log,
        TEXT("[Dodge] TryActivateAbilitiesByTag(Ability.Dodge) => %s"),
        bSuccess ? TEXT("Success") : TEXT("Fail"));
#endif
}


void ANonCharacterBase::StartAttackAlignToCamera()
{
    if (AController* LocalController = GetController())
    {
        const FRotator ControlRot = LocalController->GetControlRotation();
        const FRotator Current = GetActorRotation();

        // Yaw만 카메라에 맞추고 Pitch/Roll은 현재 유지
        AttackAlignTargetRot = Current;
        AttackAlignTargetRot.Yaw = ControlRot.Yaw;

        bAttackAlignActive = true;
    }
}

void ANonCharacterBase::UpdateAttackAlign(float DeltaSeconds)
{
    if (!bAttackAlignActive)
        return;

    const FRotator Current = GetActorRotation();

    // 부드러운 회전 (Tick마다 호출)
    const FRotator NewRot = FMath::RInterpTo(
        Current,
        AttackAlignTargetRot,
        DeltaSeconds,
        AttackAlignSpeed      // BP에서 15~30 정도로 튜닝
    );

    SetActorRotation(NewRot);

    const float DeltaYaw =
        FMath::Abs(FMath::FindDeltaAngleDegrees(NewRot.Yaw, AttackAlignTargetRot.Yaw));

    // 어느 정도 맞춰지면 정렬 종료
    if (DeltaYaw < 1.f)
    {
        bAttackAlignActive = false;
    }
}
