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
    // IAbilitySystemInterface 우선
    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(this))
    {
        ASC = ASI->GetAbilitySystemComponent();
    }
    // 아니면 컴포넌트에서 찾기
    if (!ASC)
    {
        ASC = FindComponentByClass<UAbilitySystemComponent>();
    }

    if (!ASC)
    {
        UE_LOG(LogTemp, Error, TEXT("[BeginPlay] AbilitySystemComponent not found on %s"), *GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BeginPlay] ASC cached: %s"), *GetNameSafe(ASC));
    }

    Super::BeginPlay();

    // 장착 스탠스 초기화
    CachedArmedStance = DefaultArmedStance;
    SyncEquippedStanceFromEquipment();

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        WalkSpeed_Default = Move->MaxWalkSpeed;
    }
    RefreshWeaponStance();

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

    if (!EquipmentComp)
    {
        EquipmentComp = FindComponentByClass<UEquipmentComponent>();
    }

    StartStaminaRegenLoop();

    // ── 시작 시점에 이미 HP가 0 이하일 수도 있으니 즉시 확인 ──
    if (!bDied && AttributeSet && AttributeSet->GetHP() <= 0.f)
    {
        HandleDeath();
    }
}

void ANonCharacterBase::ToggleArmed()
{
    SetArmed(!bArmed);
}


void ANonCharacterBase::SetEquippedStance(EWeaponStance NewStance)
{
    CachedArmedStance = NewStance;
    UE_LOG(LogTemp, Warning, TEXT("[EquippedStance] Set to %s"), *UEnum::GetValueAsString(CachedArmedStance));
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

    // 여기서 RefreshWeaponStance() 호출하면 Sheathe 시 Unequip 못 찾음 → 금지

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
                UE_LOG(LogTemp, Warning, TEXT("[SetArmed] Sync: PrevForMontage=%s | Equip=%s | Unequip=%s"),
                    *UEnum::GetValueAsString(PrevStanceForMontage),
                    *GetNameSafe(CurSet.Equip),
                    *GetNameSafe(CurSet.Unequip));

                UAnimMontage* Montage = bArmed ? CurSet.Equip : CurSet.Unequip;

                if (Montage)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[SetArmed] bArmed=%d | UsePrev=%s | Montage=%s"),
                        bArmed ? 1 : 0,
                        *UEnum::GetValueAsString(PrevStanceForMontage),
                        *GetNameSafe(Montage));

                    Anim->Montage_Play(Montage, 1.0f);

                    // 몽타주 종료 시 최종 스탠스/태그 반영
                    FOnMontageEnded End;
                    End.BindLambda([this, bTargetArmed = bArmed](UAnimMontage* M, bool bInterrupted)
                        {
                            // 무장 완료 → 현재 포즈 스탠스를 '장착 스탠스'로
                            // 해제 완료 → 현재 포즈 스탠스를 'Unarmed'로
                            RefreshWeaponStance(); // 내부에서 bArmed 보고 적절히 설정

                            if (ASC)
                            {
                                const FGameplayTag TagArmed = FGameplayTag::RequestGameplayTag(TEXT("State.Armed"));
                                if (bTargetArmed)
                                {
                                    if (!ASC->HasMatchingGameplayTag(TagArmed))
                                    {
                                        ASC->AddLooseGameplayTag(TagArmed);
                                        UE_LOG(LogTemp, Warning, TEXT("[SetArmed] Added Tag: State.Armed (on montage end)"));
                                    }
                                }
                                else
                                {
                                    if (ASC->HasMatchingGameplayTag(TagArmed))
                                    {
                                        ASC->RemoveLooseGameplayTag(TagArmed);
                                        UE_LOG(LogTemp, Warning, TEXT("[SetArmed] Removed Tag: State.Armed (on montage end)"));
                                    }
                                }
                            }
                        });
                    Anim->Montage_SetEndDelegate(End, Montage);
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("[SetArmed] Montage is NULL for bArmed=%d, PrevForMontage=%s (Equip=%s, Unequip=%s)"),
                        bArmed ? 1 : 0,
                        *UEnum::GetValueAsString(PrevStanceForMontage),
                        *GetNameSafe(CurSet.Equip),
                        *GetNameSafe(CurSet.Unequip));

                    // 실패 시 즉시 스탠스/태그만 정리
                    RefreshWeaponStance();
                    if (ASC)
                    {
                        const FGameplayTag TagArmed = FGameplayTag::RequestGameplayTag(TEXT("State.Armed"));
                        if (bArmed) ASC->AddLooseGameplayTag(TagArmed);
                        else        ASC->RemoveLooseGameplayTag(TagArmed);
                    }
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

    UpdateStrafeYawFollowBySpeed();
    UpdateDirectionalSpeed();
    UpdateGuardDirAndSpeed();

    if (bGuarding)
    {
        const FVector V = GetVelocity();
        if (FVector(V.X, V.Y, 0.f).SizeSquared() < 1.f)
        {
            const float TurnInterpSpeed = 8.f;
            const FRotator Target(0.f, GetControlRotation().Yaw, 0.f);
            SetActorRotation(FMath::RInterpTo(GetActorRotation(), Target, DeltaSeconds, TurnInterpSpeed));
        }
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

    if (!bStrafeMode)
    {
        bFollowCameraYawNow = false;
        bUseControllerRotationYaw = false;
        Move->bUseControllerDesiredRotation = false;
        Move->bOrientRotationToMovement = true;
        return;
    }

    bool bMoving = GetVelocity().Size2D() > YawFollowEnableSpeed && !Move->IsFalling();
    bool bShouldFollowCamera = bOnlyFollowCameraWhenMoving ? bMoving : true;

    if (bFollowCameraYawNow != bShouldFollowCamera)
    {
        bFollowCameraYawNow = bShouldFollowCamera;

        Move->bOrientRotationToMovement = false;

        bUseControllerRotationYaw = bShouldFollowCamera;
        Move->bUseControllerDesiredRotation = bShouldFollowCamera;

        Move->RotationRate = FRotator(0.f, bShouldFollowCamera ? 720.f : 540.f, 0.f);
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
    UE_LOG(LogTemp, Warning, TEXT("[Attack] HandleAttackInput called"));
    if (IsComboWindowOpen())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Attack] Combo Window Open → BufferComboInput"));
        BufferComboInput();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Attack] Combo Window Closed → TryActivateCombo"));
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

    for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
    {
        if (AbilityClass)
        {
            AbilitySystemComponent->GiveAbility(
                FGameplayAbilitySpec(AbilityClass, 1, static_cast<int32>(AbilitySystemComponent->GetActivatableAbilities().Num()), this));
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

    UE_LOG(LogTemp, Warning, TEXT("[Character] ComboWindow %s"), bOpen ? TEXT("OPEN") : TEXT("CLOSE"));
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
        UE_LOG(LogTemp, Error, TEXT("[Combo] AbilitySystemComponent is NULL"));
        return;
    }

    const FGameplayTag ActiveComboTag = FGameplayTag::RequestGameplayTag(FName("Ability.Active.Combo"));
    if (AbilitySystemComponent->HasMatchingGameplayTag(ActiveComboTag))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Combo] Combo ability already active → skip starting Combo1"));
        return;
    }

#if WITH_EDITOR
    UE_LOG(LogTemp, Warning, TEXT("[Combo] Abilities registered in ASC:"));
    for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
    {
        if (Spec.Ability)
        {
            UE_LOG(LogTemp, Warning, TEXT(" - Ability: %s"), *Spec.Ability->GetName());
            TArray<FGameplayTag> AssetTagsArr;
            Spec.Ability->GetAssetTags().GetGameplayTagArray(AssetTagsArr);
            FString TagList;
            for (const FGameplayTag& Tag : AssetTagsArr)
            {
                TagList += Tag.ToString() + TEXT(", ");
            }
            UE_LOG(LogTemp, Warning, TEXT("   AssetTags: %s"), *TagList);
        }
    }
#endif

    const FGameplayTag Combo1Tag = FGameplayTag::RequestGameplayTag(FName("Ability.Combo1"));
    UE_LOG(LogTemp, Warning, TEXT("[Combo] TryActivate Combo1")); // ← 여기! TEXT("...")로 고침
    const bool bSuccess = AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(Combo1Tag));
    UE_LOG(LogTemp, Warning, TEXT("[Combo] TryActivate Combo1: %s"), bSuccess ? TEXT("Success") : TEXT("Failed"));
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

        UE_LOG(LogTemp, Warning, TEXT("GainExp: CurrentExp = %f, MaxExp = %f, Incoming = %f"), CurrentExp, MaxExp, Amount);

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
                UE_LOG(LogTemp, Warning, TEXT("[ApplyDamageAt] DODGE popup spawned at %s"), *WorldLocation.ToString());
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

bool ANonCharacterBase::CanDodge() const
{
    const UCharacterMovementComponent* Move = GetCharacterMovement();
    if (!Move || Move->IsFalling()) return false;
    return true;
}

EEightDir ANonCharacterBase::ComputeEightDirFromInput(const FVector2D& Input2D) const
{
    if (Input2D.SizeSquared() < KINDA_SMALL_NUMBER)
    {
        return EEightDir::F;
    }

    const FRotator CtrlYaw(0.f, GetControlRotation().Yaw, 0.f);
    const FVector Forward = UKismetMathLibrary::GetForwardVector(CtrlYaw);
    const FVector Right = UKismetMathLibrary::GetRightVector(CtrlYaw);

    const FVector Desired = (Forward * Input2D.X + Right * Input2D.Y).GetSafeNormal2D();

    const FVector AF = GetActorForwardVector().GetSafeNormal2D();
    const float   Dot = FVector::DotProduct(AF, Desired);
    const float   Cross = FVector::CrossProduct(AF, Desired).Z;
    const float   Deg = FMath::RadiansToDegrees(FMath::Atan2(Cross, Dot));

    if (Deg > -22.5f && Deg <= 22.5f)         return EEightDir::F;
    else if (Deg > 22.5f && Deg <= 67.5f)    return EEightDir::FR;
    else if (Deg > 67.5f && Deg <= 112.5f)   return EEightDir::R;
    else if (Deg > 112.5f && Deg <= 157.5f)   return EEightDir::BR;
    else if (Deg > 157.5f || Deg <= -157.5f)  return EEightDir::B;
    else if (Deg > -157.5f && Deg <= -112.5f)  return EEightDir::BL;
    else if (Deg > -112.5f && Deg <= -67.5f)   return EEightDir::L;
    else                                      return EEightDir::FL;
}

static int32 EightDirToIndex(EEightDir D)
{
    switch (D)
    {
    case EEightDir::F:  return 0;
    case EEightDir::FR: return 1;
    case EEightDir::R:  return 2;
    case EEightDir::BR: return 3;
    case EEightDir::B:  return 4;
    case EEightDir::BL: return 5;
    case EEightDir::L:  return 6;
    default:            return 7; // FL
    }

}

void ANonCharacterBase::PreDodgeCancelOptions() const
{
    if (const USkeletalMeshComponent* MeshComp = GetMesh())
    {
        if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
        {
            Anim->Montage_StopGroupByName(0.02f, FName("DefaultGroup"));
        }
    }
}

void ANonCharacterBase::PlayDodgeMontage(UAnimMontage* Montage) const
{
    if (!Montage) return;
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
        {
            Anim->Montage_Play(Montage);
        }
    }
}

void ANonCharacterBase::RequestDodge2D(const FVector2D& Input2D)
{
    // SP 소모 체크
    if (!ConsumeStamina(DodgeStaminaCost))
        return;
    if (!CanDodge())
        return;

    if (bGuarding)
    {
        const_cast<ANonCharacterBase*>(this)->StopGuard();
    }

    PreDodgeCancelOptions();

    // 방향 → 인덱스(0~7)
    const EEightDir Dir = ComputeEightDirFromInput(Input2D);
    const int32 DirIdx = EightDirToIndex(Dir);

    // AnimBP에서 무기별/스탠스별 회피 꺼내오기
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
        {
            if (UNonAnimInstance* NonAnim = Cast<UNonAnimInstance>(Anim))
            {
                UAnimMontage* M = NonAnim->GetDodgeByDirIndex(DirIdx);
                PlayDodgeMontage(M);
                return;
            }
        }
    }

    // 폴백 실패 시 아무 것도 안 함
}

// ===== Stamina =====

bool ANonCharacterBase::HasEnoughStamina(float Cost) const
{
    const float CurSP = (AttributeSet ? AttributeSet->GetSP() : 0.f);
    return CurSP >= FMath::Max(Cost, MinStaminaToDodge);
}

bool ANonCharacterBase::ConsumeStamina(float Amount)
{
    if (Amount <= 0.f || !AbilitySystemComponent || !AttributeSet)
        return false;

    if (!HasEnoughStamina(Amount))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Dodge] Not enough Stamina. Need %.1f, Have %.1f"),
            Amount, AttributeSet->GetSP());
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
            UE_LOG(LogTemp, Warning, TEXT("[EquippedStance] Sync from Equipment → %s"),
                *UEnum::GetValueAsString(CachedArmedStance));
        }
    }
}
