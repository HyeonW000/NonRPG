#include "Character/NonCharacterBase.h"
#include "Ability/NonAbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Ability/GA_ComboBase.h"
#include "UI/QuickSlot/QuickSlotManager.h"
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
#include "Core/NonUIManagerComponent.h"
#include "System/SaveGameSubsystem.h"
#include "Data/LevelData.h"
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

    UIManagerComp = CreateDefaultSubobject<UNonUIManagerComponent>(TEXT("UIManagerComp"));
    QuickSlotManager = CreateDefaultSubobject<UQuickSlotManager>(TEXT("QuickSlotManager"));

    // [Changed] 무기 들었을(StrafeMode) 때는 움직이지 않아도 항상 카메라 방향 보기
    bOnlyFollowCameraWhenMoving = false;
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
                    if (UIManagerComp)
                    {
                        UIManagerComp->UpdateHP(Data.NewValue, AttributeSet->GetMaxHP());
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
                    if (UIManagerComp)
                    {
                        UIManagerComp->UpdateMP(Data.NewValue, AttributeSet->GetMaxMP());
                    }
                });

        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetSPAttribute())
            .AddLambda([this](const FOnAttributeChangeData& Data)
                {
                    if (UIManagerComp)
                    {
                        UIManagerComp->UpdateSP(Data.NewValue, AttributeSet->GetMaxSP());
                    }
                });

        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetLevelAttribute())
            .AddLambda([this](const FOnAttributeChangeData& Data)
                {
                    if (UIManagerComp)
                    {
                        UIManagerComp->UpdateLevel(static_cast<int32>(Data.NewValue));
                    }
                });

        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetExpAttribute())
            .AddLambda([this](const FOnAttributeChangeData& Data)
                {
                    if (UIManagerComp)
                    {
                        if (Data.NewValue >= AttributeSet->GetExpToNextLevel())
                        {
                            LevelUp();
                        }
                        UIManagerComp->UpdateEXP(Data.NewValue, AttributeSet->GetExpToNextLevel());
                    }
                });
    }

    // ── 초기 UI 값 세팅 (플레이어 컨트롤일 때만) ──
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (AbilitySystemComponent && AttributeSet && UIManagerComp)
        {
            UIManagerComp->UpdateHP(AttributeSet->GetHP(), AttributeSet->GetMaxHP());
            UIManagerComp->UpdateMP(AttributeSet->GetMP(), AttributeSet->GetMaxMP());
            UIManagerComp->UpdateSP(AttributeSet->GetSP(), AttributeSet->GetMaxSP());
            UIManagerComp->UpdateEXP(AttributeSet->GetExp(), AttributeSet->GetExpToNextLevel());
            UIManagerComp->UpdateLevel(static_cast<int32>(AttributeSet->GetLevel()));
        }
    }

    if (!EquipmentComp) EquipmentComp = FindComponentByClass<UEquipmentComponent>();
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

    // [New] 게임 시작 시 자동 로드 Try
    if (USaveGameSubsystem* SaveSys = GetGameInstance()->GetSubsystem<USaveGameSubsystem>())
    {
        SaveSys->LoadGame();
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

    // 1) 무장 플래그 갱신
    bArmed = bNewArmed;

    // 2) 장착된 무기 기준으로 스탠스 맞추기
    SyncEquippedStanceFromEquipment();

    // 3) 스탠스/스트레이프 정리
    RefreshWeaponStance();
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

    // [Changed] TIP 로직 삭제/비활성화 -> 항상 스트레이프 업데이트 호출
    UpdateStrafeYawFollowBySpeed();

    UpdateDirectionalSpeed();
    UpdateGuardDirAndSpeed();

    /* TIP Logic Removed
    if (!bEnableTIP_Armed) return;
    ...
    */
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

    // 1. [Highest Priority] 가드 중: 무조건 즉시 회전 (공격 후 딜레이 무시)
    if (bGuarding)
    {
        bUseControllerRotationYaw = true;
        Move->bUseControllerDesiredRotation = true;
        Move->bOrientRotationToMovement = false;
        Move->RotationRate = FRotator(0.f, 720.f, 0.f);

        // 다른 잠금 상태 강제 해제
        bRotationLockedByAbility = false;
        bAlignYawAfterRotationLock = false;
        bFollowCameraYawNow = true;
        return;
    }

    // 2. 태그 확인 (스킬, 공격, 회피 등)
    bool bHasLockTag = false;
    if (AbilitySystemComponent)
    {
        static const FGameplayTag DodgeTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dodge"));
        static const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attack"));
        static const FGameplayTag ComboTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Active.Combo"));
        static const FGameplayTag SkillTag = FGameplayTag::RequestGameplayTag(TEXT("State.Skill"));

        const bool bDodge = AbilitySystemComponent->HasMatchingGameplayTag(DodgeTag);
        const bool bAttack = AbilitySystemComponent->HasMatchingGameplayTag(AttackTag);
        const bool bCombo = AbilitySystemComponent->HasMatchingGameplayTag(ComboTag);
        const bool bSkill = AbilitySystemComponent->HasMatchingGameplayTag(SkillTag);

        bHasLockTag = (bDodge || bAttack || bCombo || bSkill);
    }

    // 3. 어빌리티 사용 중: 회전 잠금
    if (bHasLockTag)
    {
        if (!bRotationLockedByAbility)
        {
            bRotationLockedByAbility = true;
            bAlignYawAfterRotationLock = false;
        }

        bFollowCameraYawNow = false;
        bUseControllerRotationYaw = false;
        Move->bUseControllerDesiredRotation = false;
        Move->bOrientRotationToMovement = false;
        Move->RotationRate = FRotator(0.f, 0.f, 0.f);
        return;
    }

    // 4. 어빌리티 종료 직후: 정면 정렬 (Alignment)
    if (bRotationLockedByAbility)
    {
        bRotationLockedByAbility = false;
        bAlignYawAfterRotationLock = true;
    }

    if (bAlignYawAfterRotationLock)
    {
        const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
        const FRotator Current = GetActorRotation();
        FRotator Target = GetControlRotation();
        Target.Pitch = Current.Pitch;
        Target.Roll = Current.Roll;

        const FRotator NewRot = FMath::RInterpTo(Current, Target, DeltaSeconds, RotationAlignInterpSpeed);
        SetActorRotation(NewRot);

        const float DeltaYaw = FMath::Abs(FMath::FindDeltaAngleDegrees(NewRot.Yaw, Target.Yaw));
        if (DeltaYaw < 1.f)
        {
            bAlignYawAfterRotationLock = false; // 정렬 완료
        }
        else
        {
            // 정렬 중에는 조작 회전 차단
            bFollowCameraYawNow = false;
            bUseControllerRotationYaw = false;
            Move->bUseControllerDesiredRotation = false;
            Move->bOrientRotationToMovement = false;
            return;
        }
    }

    // 5. [Base Locomotion] 전투(Armed) vs 비전투(Unarmed)
    if (bStrafeMode) // Armed
    {
        // [New] Input 기반 스트레이프 Check
        // 입력이 있으면 -> 즉시 스트레이프(카메라 정렬)
        // 입력이 없으면 -> 정렬 끔(Idle Free Look)
        const FVector InputVec = GetLastMovementInputVector();
        const bool bHasInput = (InputVec.SizeSquared() > KINDA_SMALL_NUMBER);

        if (bHasInput)
        {
            if (!bFollowCameraYawNow)
            {
                bFollowCameraYawNow = true;
                
                // [Smooth Turn] 
                // bUseControllerRotationYaw = true; // 이거면 즉시 스냅 (딱딱함)
                // 대신 아래 조합으로 부드럽게 회전
                bUseControllerRotationYaw = false; 
                Move->bUseControllerDesiredRotation = true; 
                
                Move->bOrientRotationToMovement = false;
                Move->RotationRate = FRotator(0.f, 540.f, 0.f); // 속도 조절 (720 -> 540)
            }
        }
        else
        {
             // 입력 없으면 카메라만 돌릴 수 있게 회전 잠금 해제
             if (bFollowCameraYawNow)
             {
                bFollowCameraYawNow = false;
                bUseControllerRotationYaw = false;
                Move->bUseControllerDesiredRotation = false;
                Move->bOrientRotationToMovement = false; 
             }
        }
    }
    else // Unarmed
    {
        // 무조건 이동 방향 바라보기 (Free Look)
        if (bFollowCameraYawNow)
        {
            bFollowCameraYawNow = false;
            bUseControllerRotationYaw = false;
            Move->bUseControllerDesiredRotation = false;
            Move->bOrientRotationToMovement = true;
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
    if (!AbilitySystemComponent)
    {
        return;
    }

    // 1) 기본 스탯 초기화 (GE_StatInit)
    if (DefaultAttributeEffect)
    {
        FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
        EffectContext.AddSourceObject(this);

        FGameplayEffectSpecHandle NewHandle =
            AbilitySystemComponent->MakeOutgoingSpec(DefaultAttributeEffect, 1.0f, EffectContext);

        if (NewHandle.IsValid())
        {
            AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*NewHandle.Data.Get(), AbilitySystemComponent);
        }
    }

    // 2) 스태미나 리젠 GE 적용 (GE_SPRegen 같은 것)
    if (StaminaRegenEffectClass)
    {
        FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
        EffectContext.AddSourceObject(this);

        FGameplayEffectSpecHandle RegenHandle =
            AbilitySystemComponent->MakeOutgoingSpec(StaminaRegenEffectClass, 1.0f, EffectContext);

        if (RegenHandle.IsValid())
        {
            AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*RegenHandle.Data.Get(), AbilitySystemComponent);
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

    // 레벨업은 서버에서만 처리되게 하는 게 안전
    if (!HasAuthority())
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
        
        // 1. Max 수치 먼저 갱신
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetMaxHPAttribute(), NewData->MaxHP);
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetMaxMPAttribute(), NewData->MaxMP);

        // 2. 현재 수치 갱신 (Full 회복)
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetHPAttribute(), NewData->MaxHP);
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetMPAttribute(), NewData->MaxMP);

        // ── 스탯/스킬 포인트(어트리뷰트 기반) ──
        float CurrentStat = AttributeSet->GetStatPoint();
        float CurrentSkill = AttributeSet->GetSkillPoint();
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetStatPointAttribute(), CurrentStat + 5.f);
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetSkillPointAttribute(), CurrentSkill + 5.f);

        // ── 스킬 매니저 쪽 스킬 포인트(스킬트리용) ──
        if (SkillMgr)
        {
            // 레벨업당 스킬포인트
            SkillMgr->AddSkillPoints(5);
        }

        UIManagerComp->UpdateHP(NewData->MaxHP, NewData->MaxHP);
        UIManagerComp->UpdateMP(NewData->MaxMP, NewData->MaxMP);
        UIManagerComp->UpdateEXP(AttributeSet->GetExp(), NewData->ExpToNextLevel);
        UIManagerComp->UpdateLevel(NewLevel);
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

        if (UIManagerComp)
        {
            UIManagerComp->UpdateEXP(NewExp, AttributeSet->GetExpToNextLevel());
            UIManagerComp->UpdateLevel(AttributeSet->GetLevel());
        }
    }
}

void ANonCharacterBase::SetLevelAndRefreshStats(int32 NewLevel)
{
    if (!AbilitySystemComponent || !AttributeSet || !LevelDataTable) return;

    // 1. 레벨 데이터 조회
    FLevelData* NewData = LevelDataTable->FindRow<FLevelData>(*FString::FromInt(NewLevel), TEXT(""));
    if (!NewData) return;

    // 2. 레벨 및 Max 스탯 강제 설정 (GE 없이 직접 값 설정)
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetLevelAttribute(), static_cast<float>(NewLevel));
    // MaxHP/MaxMP/ExpToNextLevel 등을 데이터 테이블 값으로 갱신 (SetByCaller 대신 Base값 직접 수정)
    // 주의: AttributeSet에 해당 Attribute에 대한 Setter나 Base set이 가능해야 함. 
    // 여기서는 InitAttribute와 유사하게 BaseValue를 설정.
    
    // 만약 MaxHP/MaxMP가 Attribute라면 아래와 같이 설정
    // Data.MaxHP -> AttributeSet->GetMaxHPAttribute()
    
    // 하지만 현재 구조상 LevelUp을 보면 GE_LevelUp을 써서 Spec.Data->SetSetByCallerMagnitude로 처리함.
    // 여기서도 동일하게 GE를 적용하되 "레벨업 효과"만 빼고 "스탯 갱신"만 할 수도 있고,
    // 아니면 단순히 BaseValue를 직접 박아버리는게 가장 깔끔함 (로드니까).

    // Data.MaxHP, Data.MaxMP, Data.ExpToNextLevel 값이 필요함.
    // NonAttributeSet에 정의된 Attribute 이름 확인 필요. (InitAtribues 참고)
    // 여기서는 LevelUp 로직을 참고하여 GE를 쓰지 않고 값만 맞춤.

    // *중요*: ExpToNextLevel이 갱신되어야 Load시 Exp >= ExpToNextLevel 체크를 통과하지 않음.
    
    // NOTE: AttributeSet의 정확한 Attribute Getter 필요. 
    // 가정: GetMaxHPAttribute(), GetMaxMPAttribute(), GetExpToNextLevelAttribute()

    FGameplayEffectContextHandle Context; // 빈 컨텍스트
    // GE_LevelUp을 사용하여 스탯만 적용 (레벨 증가는 아님, 그냥 해당 레벨의 스탯 적용)
    // NewLevel에 해당하는 스탯을 적용.

    // 단순하게 BaseValue 설정
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetMaxHPAttribute(), NewData->MaxHP);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetMaxMPAttribute(), NewData->MaxMP);
    
    // [Fix] 현재 체력/마나도 Max 값으로 맞춰줍니다. (저장된 현재 체력이 없다면 풀피로 시작하는 것이 일반적)
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetHPAttribute(), NewData->MaxHP);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetMPAttribute(), NewData->MaxMP);
    
    // ExpToNextLevel은 보통 Attribute가 아니라면? -> 확인해보니 AttributeSet에 있음 (LevelUp 함수 참고)
    // LevelUp 함수: Spec.Data->SetSetByCallerMagnitude(..., NewData->ExpToNextLevel);
    // 즉 "Data.ExpToNextLevel" 태그로 매핑됨.
    
    // 직접 Attribute Base Value 설정이 제일 확실함.
    // AttributeSet->GetExpToNextLevelAttribute() 가 있다면. (보통 매크로로 생성됨)
    
    // -> 일단 코드를 안전하게 하기 위해 GE_LevelUp을 쓰지 않고 직접 값 주입 시도.
    // 전제: NonAttributeSet에 InitFromMetaDataTable 같은게 있다면 좋겠지만 없으므로 수동 설정.
    
    // *중요 체크*: NonAttributeSet.h를 못봐서 정확한 Attribute 접근자가 있는지 모름. 
    // 하지만 보통 GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) 매크로를 씀.
    // -> GetExpToNextLevelAttribute() 확인 필요. -> LevelUp함수에서는 안쓰고 GE로 넘김.
    // -> 그래도 ASC->SetNumericAttributeBase(AttributeSet->GetExpToNextLevelAttribute(), ...) 는 가능.

    // (확인된 정보: LevelUp 함수에서 SetSetByCallerMagnitude... )
    // 따라서 AttributeSet 헤더 확인 없이도 안전하게 하려면 FindAttribute를 써야하나...
    // 아니면 그냥 가장 일반적인 패턴인 Get...Attribute() 시도.
    
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetExpToNextLevelAttribute(), NewData->ExpToNextLevel);

    // 현재 HP/MP도 Max로 채워줄지? -> 로드니까 저장된거 쓰겠지만, Max가 늘어나면 비율 조정 혹은 그대로.
    // 여기선 Max만 세팅. 현재 체력은 LoadGame에서 별도로 로드하거나(저장했다면), 아니면 Max로 채움.
    // 보통 초기화니까 Max로 채워주는게 맞을수도 있지만, "로드" 과정의 일부라면 Max만 세팅하고 Cur는 나중에 로드된 값 덮어쓰기.
    
    // 로드 시점: 
    // 1. SetLevelAndRefreshStats(SavedLevel) -> Max 스탯 갱신
    // 2. ASC->SetNumericAttributeBase(Exp, SavedExp)
    // 3. (Optional) HP/MP 복구 (저장 안했다면 Full)

    if (UIManagerComp)
    {
        UIManagerComp->UpdateLevel(NewLevel);
        UIManagerComp->UpdateHP(NewData->MaxHP, NewData->MaxHP);
        UIManagerComp->UpdateMP(NewData->MaxMP, NewData->MaxMP);
        UIManagerComp->UpdateEXP(AttributeSet->GetExp(), NewData->ExpToNextLevel);
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

void ANonCharacterBase::Jump()
{
    Super::Jump();

    if (AbilitySystemComponent)
    {
        static const FGameplayTag JumpTag =
            FGameplayTag::RequestGameplayTag(TEXT("State.Jump"));

        AbilitySystemComponent->AddLooseGameplayTag(JumpTag);
    }
}

void ANonCharacterBase::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);

    if (AbilitySystemComponent)
    {
        static const FGameplayTag JumpTag =
            FGameplayTag::RequestGameplayTag(TEXT("State.Jump"));

        AbilitySystemComponent->RemoveLooseGameplayTag(JumpTag);
    }
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
}

void ANonCharacterBase::StopGuard()
{
    if (!bGuarding) return;
    bGuarding = false;

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

void ANonCharacterBase::ApplyDamageAt(float Amount, AActor* DamageInstigator, const FVector& WorldLocation, FGameplayTag ReactionTag)
{
    // 맞으면 전투 상태 진입/갱신
    EnterCombatState();

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
    OnGotHit(Amount, DamageInstigator, WorldLocation, ReactionTag);
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

void ANonCharacterBase::OnGotHit(float Damage, AActor* InstigatorActor, const FVector& ImpactPoint, FGameplayTag ReactionTag)
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


    SetLifeSpan(5.f);
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

    

    // Ability.Dodge 태그 가진 GA들 발동
    static const FGameplayTag DodgeAbilityTag =
        FGameplayTag::RequestGameplayTag(TEXT("Ability.Dodge"));

    FGameplayTagContainer DodgeTagContainer;
    DodgeTagContainer.AddTag(DodgeAbilityTag);

    const bool bSuccess = AbilitySystemComponent->TryActivateAbilitiesByTag(DodgeTagContainer);
}

void ANonCharacterBase::StartAttackAlignToCamera()
{
    if (FollowCamera)
    {
        AttackAlignTargetRot = FRotator(0.f, FollowCamera->GetComponentRotation().Yaw, 0.f);
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

void ANonCharacterBase::SetForceFullBody(bool bEnable)
{
    if (bEnable)
    {
        // 풀바디 요청 추가
        ++ForceFullBodyRequestCount;
    }
    else
    {
        // 풀바디 요청 해제
        ForceFullBodyRequestCount = FMath::Max(0, ForceFullBodyRequestCount - 1);
    }

    // 하나라도 요청이 남아 있으면 true
    bForceFullBody = (ForceFullBodyRequestCount > 0);
}

// 전투 이벤트(공격/피격/어그로 발생 시 호출)
void ANonCharacterBase::EnterCombatState()
{
    // 서버에서만 처리 (권장)
    if (!HasAuthority() || !AbilitySystemComponent) return;

    const FGameplayTag CombatTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combat"));

    if (!AbilitySystemComponent->HasMatchingGameplayTag(CombatTag))
    {
        AbilitySystemComponent->AddLooseGameplayTag(CombatTag);
    }

    GetWorldTimerManager().ClearTimer(CombatStateTimerHandle);
    GetWorldTimerManager().SetTimer(
        CombatStateTimerHandle,
        this,
        &ANonCharacterBase::LeaveCombatState,
        CombatStateDuration,
        false
    );
}

void ANonCharacterBase::LeaveCombatState()
{
    if (!HasAuthority() || !AbilitySystemComponent) return;

    const FGameplayTag CombatTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combat"));
    AbilitySystemComponent->RemoveLooseGameplayTag(CombatTag);

    GetWorldTimerManager().ClearTimer(CombatStateTimerHandle);
}

bool ANonCharacterBase::IsInCombat() const
{
    if (!AbilitySystemComponent) return false;
    return AbilitySystemComponent->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Combat")));
}

void ANonCharacterBase::SaveGameTest()
{
    if (USaveGameSubsystem* SaveSys = GetGameInstance()->GetSubsystem<USaveGameSubsystem>())
    {
        SaveSys->SaveGame();
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("SaveGameTest: Saved!"));
    }
}

void ANonCharacterBase::ResetGameTest()
{
    if (USaveGameSubsystem* SaveSys = GetGameInstance()->GetSubsystem<USaveGameSubsystem>())
    {
        SaveSys->DeleteSaveGame();
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("ResetGameTest: Save Deleted. Restart Game!"));
    }
}

