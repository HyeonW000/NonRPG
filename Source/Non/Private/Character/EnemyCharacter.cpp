#include "Character/EnemyCharacter.h"
#include "Core/NonGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h" // [Fix] Required for DOREPLIFETIME

#include "Ability/NonAbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h" // [Fix] Required for accessing UGameplayAbility class
#include "Ability/GA_EnemyAttack.h"
#include "Ability/GA_EnemyTurn.h"
#include "Ability/GA_Death.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagsManager.h"
#include "GameplayTagContainer.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/ANS_AttackHitbox.h"
#include "Animation/AnimNotify_SpawnActor.h"
#include "AnimNotifyState_MotionWarping.h"

#include "UI/EnemyHPBarWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Character/NonCharacterBase.h"
#include "Inventory/InventoryComponent.h"
#include "Core/NonPlayerController.h"
#include "System/PartyComponent.h"
#include "Combat/CombatSlotComponent.h"
#include "Character/EnemyCorpseComponent.h"

#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

#include "AIController.h"
#include "AI/NonAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Effects/DamageNumberActor.h"
#include "BrainComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Data/EnemyDataAsset.h"
#include "Data/BossDataAsset.h"
#include "Character/BossCharacter.h"
#include "Combat/NonDamageHelpers.h" 
#include "MotionWarpingComponent.h"
#include "Components/ArrowComponent.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"

AEnemyCharacter::AEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // AI 컨트롤러 자동 지정 (하이브리드/원거리 카이팅 및 조준 턴 이동 제어를 전담하는 ANonAIController)
    AIControllerClass = ANonAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // C++ 생성자에서 모션 워핑 컴포넌트 자동 생성
    MotionWarpingComp = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComp"));

    // [Fix] AI/Enemy는 GAS 공식 규칙상 Minimal 모드를 사용해야 모든 클라이언트에 몽타주와 어빌리티가 100% 정상 복제됩니다!
    AbilitySystemComponent = CreateDefaultSubobject<UNonAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    // [Multiplayer Fix] 몽타주 루트 모션 및 애니메이션이 네트워크 클라이언트에서 누락되지 않도록 틱 옵션 설정
    SetNetUpdateFrequency(66.f);
    SetMinNetUpdateFrequency(33.f);
    if (GetMesh())
    {
        GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    }

    AttributeSet = CreateDefaultSubobject<UNonAttributeSet>(TEXT("AttributeSet"));

    // HP Bar
    HPBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HPBar"));
    if (HPBarWidget)
    {
        HPBarWidget->SetupAttachment(GetMesh());
        HPBarWidget->SetRelativeLocation(FVector(0, 0, 120.f));
        HPBarWidget->SetDrawAtDesiredSize(true);
        HPBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
        HPBarWidget->SetTwoSided(false);
        HPBarWidget->SetPivot(FVector2D(0.5f, 0.5f));
        HPBarWidget->SetDrawSize(FIntPoint(120, 14));
        HPBarWidget->SetVisibility(false);
        HPBarWidget->SetWidgetClass(UEnemyHPBarWidget::StaticClass());
    }

    // 경계 효과 위젯 (머리 위 느낌표/효과선 이미지)
    AlertWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("AlertWidget"));
    if (AlertWidget)
    {
        AlertWidget->SetupAttachment(GetMesh());
        AlertWidget->SetRelativeLocation(FVector(0, 0, 145.f)); // HP바보다 약간 위쪽
        AlertWidget->SetDrawAtDesiredSize(true);
        AlertWidget->SetWidgetSpace(EWidgetSpace::Screen); // 카메라 정면 응시
        AlertWidget->SetTwoSided(false);
        AlertWidget->SetPivot(FVector2D(0.5f, 0.5f));
        AlertWidget->SetDrawSize(FIntPoint(100, 100));
        AlertWidget->SetVisibility(false); // 평소에는 숨김
    }

    // 이동 및 AI 충돌 회피 (RVO Avoidance)
    bUseControllerRotationYaw = false;
    if (auto* Move = GetCharacterMovement())
    {
        Move->bOrientRotationToMovement = true;
        Move->RotationRate = FRotator(0, 720, 0); // 빠른 U턴 및 방향 전환을 위해 720도로 상향
        Move->MaxWalkSpeed = 350.f;
        Move->bUseRVOAvoidance = true;
        Move->AvoidanceWeight = 0.3f; // 과도하게 밀리는 현상 완화
        Move->AvoidanceConsiderationRadius = 150.f; // 300cm에서 150cm로 축소하여 가까울 때만 부드럽게 회피
    }

    // 몬스터 캡슐 콜리전 설정 (플레이어 및 다른 몬스터와 물리적으로 겹치거나 뚫고 지나가지 않도록 Block)
    if (GetCapsuleComponent())
    {
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No; // 계단처럼 밟고 올라타기 방지
    }

    // 히트박스는 이제 BeginPlay에서 "AttackHitbox" 태그를 가진 모든 컴포넌트를 자동으로 찾아 관리합니다.
    // 따라서 에디터에서 자유롭게 추가/삭제가 가능합니다.

    //Fade
    SpawnFadeDuration = 0.6f;
    bUseSpawnFadeIn = true;

    CorpseComponent = CreateDefaultSubobject<UEnemyCorpseComponent>(TEXT("CorpseComponent"));

    InteractCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractCollision"));
    InteractCollision->SetupAttachment(GetRootComponent());
    InteractCollision->InitSphereRadius(CorpseInteractRadius);
    InteractCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InteractCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    InteractCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
}

void AEnemyCharacter::InitFromDataAsset(const UEnemyDataAsset* InData)
{
    if (!InData) return;

    EnemyData = InData;
    if (EnemyDisplayName.IsEmpty() && !InData->Name.IsEmpty())
    {
        EnemyDisplayName = InData->Name;
    }
    BaseAttack = InData->Attack;
    BaseDefense = InData->Defense;
    ExpReward = InData->ExpReward;
    // DataAsset에서 지정한 스탯들을 AttributeSet(GAS)에 직접 강제 주입
    if (AttributeSet)
    {
        AttributeSet->InitMaxHP(InData->MaxHP);
        AttributeSet->InitHP(InData->MaxHP);
        
        AttributeSet->InitAttackPower(InData->Attack);
        AttributeSet->InitDefense(InData->Defense);
    }
}

TArray<FString> AEnemyCharacter::GetEnemyRowNames() const
{
    TArray<FString> Options;
    if (EnemyDataTable)
    {
        for (const FName& RowName : EnemyDataTable->GetRowNames())
        {
            Options.Add(RowName.ToString());
        }
    }
    return Options;
}

bool AEnemyCharacter::IsBoss() const
{
    return bIsBoss || IsA<ABossCharacter>();
}

void AEnemyCharacter::InitFromDataTableRow(const FEnemyDataTableRow& InRow)
{
    EnemyDisplayName = InRow.Name;
    bIsBoss = InRow.bIsBoss;

    // 런타임 메모리용 UEnemyDataAsset 인스턴스를 즉석 생성하여 기존 시스템(루팅, 이름, 스탯 복제 등)과 100% 호환 보장!
    UEnemyDataAsset* TransientAsset = NewObject<UEnemyDataAsset>(this);
    TransientAsset->EnemyId = InRow.EnemyId;
    TransientAsset->Name = InRow.Name;
    TransientAsset->EnemyClass = InRow.EnemyClass;
    TransientAsset->MaxHP = InRow.MaxHP;
    TransientAsset->Attack = InRow.Attack;
    TransientAsset->Defense = InRow.Defense;
    TransientAsset->ExpReward = InRow.ExpReward;
    TransientAsset->DropList = InRow.DropList;

    InitFromDataAsset(TransientAsset);

    CombatStyle = InRow.CombatStyle;

    // 0) 공격 애니메이션 몽타주 및 사정거리 주입 (CombatStyle에 맞춰 자동 정제)
    if (CombatStyle != EEnemyCombatStyle::Ranged && InRow.AttackMontages.Num() > 0)
    {
        AttackMontages = InRow.AttackMontages;
    }
    else if (CombatStyle == EEnemyCombatStyle::Ranged)
    {
        AttackMontages.Empty(); // 원거리 전용 몬스터는 근접 공격 제거
    }

    if (InRow.DeathMontage)
    {
        // 보스는 BP에 세팅된 전용 사망 몽타주(예: AM_Golem_Death)가 최우선이므로 이미 지정되어 있다면 DT로 덮어쓰지 않음
        if (!IsBoss() || DeathMontage == nullptr)
        {
            DeathMontage = InRow.DeathMontage;
        }
    }

    // 0-1) 피격 애니메이션 몽타주 주입 (일반 몬스터 전용 - 약피격, 강피격, 넉다운, 스턴)
    HitMontages.Empty();
    if (InRow.HitLightMontage)
    {
        HitMontages.Add(NonTags::Effect_Hit_Light.GetTag(), InRow.HitLightMontage);
    }
    if (InRow.HitHeavyMontage)
    {
        HitMontages.Add(NonTags::Effect_Hit_Heavy.GetTag(), InRow.HitHeavyMontage);
    }
    if (InRow.KnockdownMontage)
    {
        HitMontages.Add(NonTags::Effect_Hit_KnockDown.GetTag(), InRow.KnockdownMontage);
        HitMontages.Add(NonTags::State_Knockdown.GetTag(), InRow.KnockdownMontage);
    }
    if (InRow.StunMontage)
    {
        HitMontages.Add(NonTags::State_Stunned.GetTag(), InRow.StunMontage);
        HitMontages.Add(NonTags::Effect_Hit_Stun.GetTag(), InRow.StunMontage);
    }
    // 패링 경직: 전용 몽타주 → 강피격 → 약피격 순으로 대체 (설계서 5장 16)
    {
        UAnimMontage* ParriedToUse = InRow.ParriedMontage ? InRow.ParriedMontage.Get()
            : (InRow.HitHeavyMontage ? InRow.HitHeavyMontage.Get() : InRow.HitLightMontage.Get());
        const FGameplayTag ParriedTag = NonTags::Effect_Hit_Parried.GetTag();
        if (ParriedToUse && ParriedTag.IsValid())
        {
            HitMontages.Add(ParriedTag, ParriedToUse);
        }
    }
    if (InRow.AttackRange > 0.f)
    {
        AttackRange = InRow.AttackRange;
    }
    if (CombatStyle != EEnemyCombatStyle::Melee && InRow.RangeAttackMontage)
    {
        RangeAttackMontage = InRow.RangeAttackMontage;

        // 원거리 공격 몽타주가 등록되어 있다면 원거리 어빌리티(UGA_Enemy_RangeAttack)를 자동 부여!
        if (AbilitySystemComponent && HasAuthority())
        {
            AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UGA_Enemy_RangeAttack::StaticClass(), 1, INDEX_NONE, this));
        }
    }
    else if (CombatStyle == EEnemyCombatStyle::Melee)
    {
        RangeAttackMontage = nullptr; // 근접 전용 몬스터는 원거리 공격 제거
    }
    if (InRow.RangeAttackRange > 0.f)
    {
        RangeAttackRange = InRow.RangeAttackRange;
    }
    bFleeWhenTooClose = InRow.bFleeWhenTooClose;
    if (InRow.BackstepMontage)
    {
        BackstepMontage = InRow.BackstepMontage;
    }
    AttackDamageType = InRow.AttackDamageType;
    if (InRow.AttackPowerScale > 0.f)
    {
        AttackPowerScale = InRow.AttackPowerScale;
    }
    AggroStyle = InRow.AggroStyle;
    if (InRow.CombatRunSpeed > 0.f)
    {
        CombatRunSpeed = InRow.CombatRunSpeed;
    }
    if (InRow.PatrolWalkSpeed > 0.f)
    {
        PatrolWalkSpeed = InRow.PatrolWalkSpeed;
    }
    if (InRow.AimTurnRate > 0.f)
    {
        AimTurnRate = InRow.AimTurnRate;
    }
    if (InRow.FirstAttackWindupMin >= 0.f)
    {
        FirstAttackWindupMin = InRow.FirstAttackWindupMin;
    }
    if (InRow.FirstAttackWindupMax >= 0.f)
    {
        FirstAttackWindupMax = InRow.FirstAttackWindupMax;
    }
    if (InRow.AttackCooldown >= 0.f)
    {
        AttackCooldown = InRow.AttackCooldown;
    }
    if (InRow.RangeAttackCooldown >= 0.f)
    {
        RangeAttackCooldown = InRow.RangeAttackCooldown;
    }
    if (InRow.AlertMontage)
    {
        AlertMontage = InRow.AlertMontage;
    }
    bShowAlertMark = InRow.bShowAlertMark;
    if (InRow.AlertDelay >= 0.f)
    {
        AlertDelay = InRow.AlertDelay;
    }

    // 1) 보스 전용 페이즈 데이터 에셋(BossPhaseData) 자동 초기화 (bIsBoss인 경우)
    if (InRow.BossPhaseData)
    {
        if (ABossCharacter* Boss = Cast<ABossCharacter>(this))
        {
            Boss->InitBossPhaseData(InRow.BossPhaseData);
        }
    }
}

FString AEnemyCharacter::GetEnemyName() const
{
    // 1) 네트워크 복제된 표시 이름 최우선 반환 (클라이언트에서도 100% 한글 정상 표시)
    if (!EnemyDisplayName.IsEmpty())
    {
        return EnemyDisplayName.ToString();
    }
    // 2) 로컬 EnemyData Asset이 유효한 경우
    if (EnemyData && !EnemyData->Name.IsEmpty())
    {
        return EnemyData->Name.ToString();
    }
    // 3) 데이터 테이블에서 직접 조회
    if (EnemyDataTable && !EnemyRowName.IsNone())
    {
        if (const FEnemyDataTableRow* Row = EnemyDataTable->FindRow<FEnemyDataTableRow>(EnemyRowName, TEXT("EnemyCharacter::GetEnemyName")))
        {
            return Row->Name.ToString();
        }
    }
    return TEXT("Unknown Enemy");
}

UAbilitySystemComponent* AEnemyCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    SpawnLocation = GetActorLocation();

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
        BindASCDelegates();
    }
    BindAttributeDelegates();

    // 레벨에 직접 배치되었거나 스포너 없이 스폰된 적도 DT_EnemyData로부터 완벽 자동 동기화
    if (HasAuthority())
    {
        // 1) EnemyDataTable은 BP(BP_Enemy_Base / BP_Boss_Base)에서 지정. 비어 있으면 DT 동기화 없이 BP 기본값으로 동작
        if (!EnemyDataTable)
        {
            static bool bWarnedEnemyDT = false;
            if (!bWarnedEnemyDT)
            {
                bWarnedEnemyDT = true;
                UE_LOG(LogTemp, Warning, TEXT("[Enemy] %s: EnemyDataTable이 비어 있습니다. BP Class Defaults에서 DT_EnemyData를 지정하세요"), *GetClass()->GetName());
            }
        }

        if (EnemyDataTable)
        {
            const FEnemyDataTableRow* TargetRow = nullptr;

            // 2) 인스턴스에 수동 지정된 EnemyRowName이 있으면 최우선 검색
            if (!EnemyRowName.IsNone())
            {
                TargetRow = EnemyDataTable->FindRow<FEnemyDataTableRow>(EnemyRowName, TEXT("EnemyCharacter::BeginPlay"));
            }

            // 3) 없으면 본인의 Blueprint 클래스(EnemyClass)와 일치하는 행을 DT에서 자동 매칭!
            if (!TargetRow)
            {
                for (const FName& RowName : EnemyDataTable->GetRowNames())
                {
                    if (const FEnemyDataTableRow* Row = EnemyDataTable->FindRow<FEnemyDataTableRow>(RowName, TEXT("EnemyCharacter::AutoMatchClass")))
                    {
                        if (Row->EnemyClass == GetClass())
                        {
                            TargetRow = Row;
                            EnemyRowName = RowName;
                            break;
                        }
                    }
                }
            }

            // 4) 그래도 없으면 행 이름(Dummy 등)이 클래스 이름(BP_Enemy_Dummy)에 포함되어 있는지 스마트 매칭!
            if (!TargetRow)
            {
                const FString ClassName = GetClass()->GetName();
                for (const FName& RowName : EnemyDataTable->GetRowNames())
                {
                    if (ClassName.Contains(RowName.ToString()))
                    {
                        if (const FEnemyDataTableRow* Row = EnemyDataTable->FindRow<FEnemyDataTableRow>(RowName, TEXT("EnemyCharacter::NameMatch")))
                        {
                            TargetRow = Row;
                            EnemyRowName = RowName;
                            break;
                        }
                    }
                }
            }

            // 5) 최후의 보루: DT의 첫 번째 행 자동 매칭
            if (!TargetRow)
            {
                TArray<FName> RowNames = EnemyDataTable->GetRowNames();
                if (RowNames.Num() > 0)
                {
                    TargetRow = EnemyDataTable->FindRow<FEnemyDataTableRow>(RowNames[0], TEXT("EnemyCharacter::FirstRow"));
                    EnemyRowName = RowNames[0];
                }
            }

            if (TargetRow)
            {
                InitFromDataTableRow(*TargetRow);
                EnemyDisplayName = TargetRow->Name;
            }
        }
    }

    // 레벨에 직접 배치되었거나 스포너 없이 스폰된 적 캐릭터도 스탯/어빌리티가 즉시 초기화되도록 보장
    if (HasAuthority())
    {
        InitializeAttributes();

        // AI 컨트롤러가 ANonAIController가 아니라면 교체하여 카이팅 및 조준 턴 이동 로직 활성화
        if (AController* ExistingC = GetController())
        {
            if (!ExistingC->IsA<ANonAIController>())
            {
                ExistingC->UnPossess();
                ExistingC->Destroy();
                SpawnDefaultController();
            }
        }
        else
        {
            SpawnDefaultController();
        }
    }

    if (HPBarWidget)
    {
        HPBarWidget->InitWidget();
        UpdateHPBar();
        HPBarWidget->SetVisibility(false);
    }

    // [Legacy Removed] AnimSet assignments (GAS uses GA_Death/GA_Hit)

    if (bUseSpawnFadeIn && CorpseComponent)
    {
        CorpseComponent->InitFadeMaterials();
        PlaySpawnFadeIn(SpawnFadeDuration);
    }

    // [New] 태그가 "AttackHitbox"인 모든 콜리전 컴포넌트를 찾아 배열에 담고 이벤트 연결
    TArray<UShapeComponent*> FoundComponents;
    GetComponents<UShapeComponent>(FoundComponents);
    for (UShapeComponent* Shape : FoundComponents)
    {
        if (Shape && Shape->ComponentTags.Contains(TEXT("AttackHitbox")))
        {
            ActiveHitboxes.Add(Shape);
            Shape->OnComponentBeginOverlap.AddDynamic(this, &AEnemyCharacter::OnAttackHitBegin);
            
            // 초기 상태는 꺼둠
            Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Shape->SetGenerateOverlapEvents(false);
        }
    }



    UpdateHPBarVisibility();
}

void AEnemyCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (CorpseComponent)
    {
        CorpseComponent->TickFade();
    }

    // 전투 회전(피격 중 공격자 방향 고정, 공격 준비 추적 회전)은 서버에서만 계산.
    // 클라이언트는 공격자 정보(LastDamageInstigator)/AI가 없어 다른 대상을 바라보게 되고,
    // 복제된 회전과 매 프레임 싸워 피격 시 떨림이 생김 → 서버 회전을 복제받아 보간 표시.
    if (HasAuthority())
    {
        TickCombatRotation(DeltaSeconds);
    }
}

void AEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AEnemyCharacter, EnemyDisplayName);
    DOREPLIFETIME(AEnemyCharacter, EnemyRowName);
    DOREPLIFETIME(AEnemyCharacter, AggroTarget);
    DOREPLIFETIME(AEnemyCharacter, DeathServerTime);
}

bool AEnemyCharacter::IsBackAttackMontage(const UAnimMontage* Montage) const
{
    static const FGameplayTag BackTag = NonTags::Enemy_Skill_Back.GetTag();
    if (!BackTag.IsValid())
    {
        return false;
    }

    // 1) 보스 데이터의 스킬 목록 (클라이언트에서도 같은 에셋을 가지고 있어 판정 가능)
    if (Montage)
    {
        if (const ABossCharacter* Boss = Cast<ABossCharacter>(this))
        {
            if (Boss->BossData)
            {
                for (const FBossPhaseData& PhaseData : Boss->BossData->PhaseList)
                {
                    for (const FBossSkillEntry& SkillEntry : PhaseData.Skills)
                    {
                        if (SkillEntry.Montage == Montage && SkillEntry.SkillCategoryTag.MatchesTag(BackTag))
                        {
                            return true;
                        }
                    }
                }
            }
        }
    }

    // 2) GA로 실행하는 후방 공격 (GA_Boss_BackAttack_Base 등 에셋 태그에 Enemy.Skill.Back)
    if (AbilitySystemComponent)
    {
        if (AbilitySystemComponent->HasMatchingGameplayTag(BackTag))
        {
            return true;
        }
        for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
        {
            if (Spec.IsActive() && Spec.Ability && Spec.Ability->GetAssetTags().HasTag(BackTag))
            {
                return true;
            }
        }
    }
    return false;
}

void AEnemyCharacter::InitializeAttributes()
{
    // 1) 기본 GE 적용 (공통 초기값)
    if (AbilitySystemComponent && GE_DefaultAttributes)
    {
        FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
        Ctx.AddSourceObject(this);

        if (FGameplayEffectSpecHandle Spec =
            AbilitySystemComponent->MakeOutgoingSpec(GE_DefaultAttributes, 1.f, Ctx);
            Spec.IsValid())
        {
            AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        }
    }

    // 2) EnemyDataAsset 값으로 AttributeSet 덮어쓰기 (개별 몬스터 차이)
    if (AttributeSet && EnemyData)
    {
        // MaxHP
        if (EnemyData->MaxHP > 0.f)
        {
            AttributeSet->SetMaxHP(EnemyData->MaxHP);
            AttributeSet->SetHP(EnemyData->MaxHP);     // 처음 HP = MaxHP 로 채우기
        }

        // Attack / Defense -> AttackPower / DefensePower 로 매핑
        if (EnemyData->Attack > 0.f)
        {
            AttributeSet->SetAttackPower(EnemyData->Attack);
        }

        if (EnemyData->Defense > 0.f)
        {
            AttributeSet->SetDefense(EnemyData->Defense);
        }
    }

    // 3) HPBar 갱신 + 디버그 로그
    UpdateHPBar();

    if (AttributeSet)
    {
        const float CurHP = AttributeSet->GetHP();
        const float MaxHP = AttributeSet->GetMaxHP();
    }

    // [New] 기본 어빌리티 부여 (GA_HitReaction 등) - Server Only
    if (HasAuthority() && AbilitySystemComponent)
    {
        for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
        {
            if (AbilityClass)
            {
                FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
                AbilitySystemComponent->GiveAbility(Spec);
            }
        }

        // 원거리 공격 몽타주가 등록되어 있다면 원거리 공격 어빌리티(UGA_Enemy_RangeAttack) 확실하게 부여!
        if (RangeAttackMontage)
        {
            FGameplayAbilitySpec Spec(UGA_Enemy_RangeAttack::StaticClass(), 1, INDEX_NONE, this);
            AbilitySystemComponent->GiveAbility(Spec);
        }

        // ★ 회전 몽타주(TurnLeftMontage / TurnRightMontage)가 등록되어 있거나 보스인 경우,
        // 자식 블루프린트(BP_Boss_Golem 등)에서 DefaultAbilities 배열이 오버라이드되어 GA_EnemyTurn이 빠져 있더라도 자동 부여!
        if (IsBoss() || TurnLeftMontage || TurnRightMontage)
        {
            bool bHasTurnAbility = false;
            for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
            {
                if (Spec.Ability && Spec.Ability->IsA<UGA_EnemyTurn>())
                {
                    bHasTurnAbility = true;
                    break;
                }
            }
            if (!bHasTurnAbility)
            {
                FGameplayAbilitySpec TurnSpec(UGA_EnemyTurn::StaticClass(), 1, INDEX_NONE, this);
                AbilitySystemComponent->GiveAbility(TurnSpec);
            }
        }

        // ★ 사망 어빌리티(UGA_Death)가 등록되어 있는지 검사하고, 없으면 자동 부여 보장!
        bool bHasDeathAbility = false;
        for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
        {
            if (Spec.Ability && Spec.Ability->IsA<UGA_Death>())
            {
                bHasDeathAbility = true;
                break;
            }
        }
        if (!bHasDeathAbility)
        {
            // DefaultAbilities에 사망 어빌리티가 없으면 C++ 기본 사망 어빌리티로 대체 (몽타주는 캐릭터 DeathMontage 사용)
            UE_LOG(LogTemp, Warning, TEXT("[Enemy] %s: DefaultAbilities에 사망 어빌리티가 없어 기본 UGA_Death를 부여합니다"), *GetClass()->GetName());
            FGameplayAbilitySpec DeathSpec(UGA_Death::StaticClass(), 1, INDEX_NONE, this);
            AbilitySystemComponent->GiveAbility(DeathSpec);
        }
    }
}

void AEnemyCharacter::BindAttributeDelegates()
{
    if (!AbilitySystemComponent || !AttributeSet) return;

    AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHPAttribute())
        .AddLambda([this](const FOnAttributeChangeData& Data)
            {
                if (Data.NewValue < Data.OldValue)
                {
                    EnterCombat();
                }
                UpdateHPBar();
                UpdateHPBarVisibility();

                if (Data.OldValue > 0.f && Data.NewValue <= 0.f)
                {
                    // 사망 판정 시 GA_Death/GA_Enemy_Death 100% 확실한 자동 발동 3중 보장!
                    if (AbilitySystemComponent)
                    {
                        // 1) 게임플레이 이벤트 발송 (Effect.Death)
                        FGameplayTag DeathEventTag = NonTags::Effect_Death.GetTag();
                        if (DeathEventTag.IsValid())
                        {
                            FGameplayEventData Payload;
                            Payload.EventTag = DeathEventTag;
                            Payload.Target = this;
                            AbilitySystemComponent->HandleGameplayEvent(DeathEventTag, &Payload);
                        }

                        // 2) 태그 직접 발동 (Ability.Death)
                        FGameplayTag DeathAbilityTag = NonTags::Ability_Death.GetTag();
                        if (DeathAbilityTag.IsValid())
                        {
                            FGameplayTagContainer DeathTags;
                            DeathTags.AddTag(DeathAbilityTag);
                            AbilitySystemComponent->TryActivateAbilitiesByTag(DeathTags);
                        }

                        // 3) 어빌리티 인스턴스 직접 탐색 및 발동 (CDO 태그 누락 방어)
                        for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
                        {
                            if (Spec.Ability && Spec.Ability->IsA<UGA_Death>() && !Spec.IsActive())
                            {
                                AbilitySystemComponent->TryActivateAbility(Spec.Handle);
                                break;
                            }
                        }
                    }
                }
            });
}

void AEnemyCharacter::UpdateHPBar() const
{
    if (!HPBarWidget) return;

    if (UEnemyHPBarWidget* W = Cast<UEnemyHPBarWidget>(HPBarWidget->GetUserWidgetObject()))
    {
        const float Cur = AttributeSet ? AttributeSet->GetHP() : 0.f;
        const float Max = AttributeSet ? AttributeSet->GetMaxHP() : 1.f;
        W->SetHP(Cur, Max);
    }
    else
    {

    }
}

bool AEnemyCharacter::IsDead() const
{
    if (!AttributeSet) return false;
    if (AttributeSet->GetHP() <= 0.f) return true;
    return HasDeadTag();
}
// [Updated] GAS GA_Death에서 호출됨
void AEnemyCharacter::StartDeathSequence()
{
    if (bDied) return; // 이미 죽음

    bDied = true;

    // 늦게 접속한 사람용 사망 시각 (OnRep_DeathServerTime)
    if (HasAuthority() && GetWorld())
    {
        if (const AGameStateBase* GS = GetWorld()->GetGameState())
        {
            DeathServerTime = GS->GetServerWorldTimeSeconds();
        }
    }

    // 사망 시 추적 대상 해제 (클라이언트 HP바/타겟 HUD 정리)
    SetAggroTarget(nullptr);

    // 경계 타이머 및 상태 즉시 해제
    if (AlertTimerHandle.IsValid())
    {
        GetWorldTimerManager().ClearTimer(AlertTimerHandle);
    }
    bIsAlerting = false;
    HideAlertMark();

    // [New] 모든 디버프(출혈 등) 및 스턴 상태 강제 제거
    if (AbilitySystemComponent)
    {
        FGameplayTagContainer DebuffTags;
        DebuffTags.AddTag(NonTags::State_Debuff.GetTag());
        DebuffTags.AddTag(NonTags::State_Stunned.GetTag());
        DebuffTags.AddTag(NonTags::State_HitReacting.GetTag());
        AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(DebuffTags);
    }

    // 죽자마자 바로 HP바 끄기
    if (HPBarWidget)
    {
        HPBarWidget->SetVisibility(false);
    }

    // 타겟 플레이어의 포위 슬롯 반환
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
        {
            if (AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor"))))
            {
                if (UCombatSlotComponent* SlotComp = TargetActor->FindComponentByClass<UCombatSlotComponent>())
                {
                    SlotComp->ReleaseSlot(this);
                }
            }
        }
    }

    OnEnemyDied.Broadcast(this);

    // 개인 루팅 대상자 확정 + 경험치 지급(파티 보너스 및 거리 제한 균등 분배)
    if (HasAuthority() && CorpseComponent)
    {
        CorpseComponent->OnOwnerDied(LastDamageInstigator.Get(), ExpReward);
    }
    
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
        Move->Velocity = FVector::ZeroVector;
        Move->SetAvoidanceEnabled(false);
        Move->bUseRVOAvoidance = false;
    }
    
    // AI 정지 (UnPossess는 몽타주 종료 후 FreezeDeathPose에서 수행하여 애니메이션 중단 방지!)
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (UBrainComponent* Brain = AIC->GetBrainComponent())
        {
            Brain->StopLogic("Death");
        }
        AIC->StopMovement();
    }

    // [Fix] 캡슐 크기는 원래대로 유지하여 바닥 지지를 완벽히 유지하고,
    // 서버와 모든 클라이언트 화면에서 Pawn과 Camera만 즉시 Ignore(통과) 설정!
    if (HasAuthority())
    {
        Multicast_OnDeathCollision();
    }
    else
    {
        Multicast_OnDeathCollision_Implementation();
    }

    // 시체 상호작용 가능 상태로 전환
    EnableCorpseInteraction();
}

void AEnemyCharacter::Multicast_OnDeathCollision_Implementation()
{
    // 0) 이동 컴포넌트 즉시 정지 및 물리 상호작용 차단 (다른 보스나 플레이어가 밀어내지 못하게 원천 차단)
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
        Move->Velocity = FVector::ZeroVector;
        Move->DisableMovement(); // 이동 모드 MOVE_None으로 완전 차단!
        Move->bEnablePhysicsInteraction = false; // 다른 물체가 밀 수 없도록 물리 상호작용 끄기
        Move->bPushForceUsingZOffset = false;
        Move->PushForceFactor = 0.f;
        Move->InitialPushForceFactor = 0.f;
        Move->SetAvoidanceEnabled(false);
        Move->bUseRVOAvoidance = false;
    }

    // 1) 캡슐 콜리전: 물리 충돌(Physics)을 완전히 끄고(QueryOnly), 오직 바닥(WorldStatic/Dynamic)만 Block!
    // Pawn, Vehicle, PhysicsBody, Destructible 등 다른 보스나 공격 판정에 의한 밀림 100% 원천 차단!
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
        Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        Capsule->CanCharacterStepUpOn = ECB_No;
    }

    // 2) 스켈레탈 메쉬(피직스 에셋 본 콜리전)도 동일하게 물리 시뮬레이션을 끄고 바닥만 유지!
    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        Skel->SetSimulatePhysics(false);
        Skel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Skel->SetCollisionResponseToAllChannels(ECR_Ignore);
        Skel->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        Skel->CanCharacterStepUpOn = ECB_No;
    }

    // 3) 죽은 몬스터/보스에 부착된 모든 컴포넌트(히트박스, 보석 등)의 충돌 완전 제거!
    TArray<UPrimitiveComponent*> PrimComps;
    GetComponents<UPrimitiveComponent>(PrimComps);
    for (UPrimitiveComponent* PrimComp : PrimComps)
    {
        if (PrimComp && PrimComp != InteractCollision && PrimComp != GetCapsuleComponent() && PrimComp != GetMesh())
        {
            PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            PrimComp->SetCollisionResponseToAllChannels(ECR_Ignore);
        }
    }
}

void AEnemyCharacter::StartRagdoll()
{
    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        Skel->SetCollisionProfileName(FName("Ragdoll"));
        Skel->SetAllBodiesSimulatePhysics(true);
        Skel->SetSimulatePhysics(true);
        Skel->WakeAllRigidBodies();
        Skel->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    }    
}

// [Legacy] 내부 호출용이었던 함수 -> 이제는 GA가 StartDeathSequence + Animation 처리
void AEnemyCharacter::HandleDeath()
{
    // [Fix] 상위 부모 클래스(NonCharacterBase)의 체력 감지 델리게이트가 여전히 이 함수를 몰래 호출하고 있었습니다!!
    // 여기서 강제로 래그돌을 켜버리면 뼈대가 굳어버려 GA_Death가 몽타주를 틀지 못합니다.
    // 따라서 몬스터(Enemy)는 부모의 옛날 방식 죽음을 철저히 무시하고 모두 GAS(GA_Death)에게 위임합니다.
    
    // StartDeathSequence();
    // StartRagdoll();
}

void AEnemyCharacter::OnRep_DeathServerTime()
{
    // 실시간으로 본 사망은 멀티캐스트(충돌/포즈 고정)가 처리. 늦게 접속해서 이미 죽어 있던 적만 여기서 처리.
    if (DeathServerTime < 0.f || bDied || !GetWorld())
        return;

    const AGameStateBase* GS = GetWorld()->GetGameState();
    const float Now = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    const float Len = DeathMontage ? DeathMontage->GetPlayLength() : 0.f;
    if (Now - DeathServerTime < Len + 0.5f)
        return;

    bDied = true;
    Multicast_OnDeathCollision_Implementation();
    Multicast_FreezeDeathPose_Implementation();
}

void AEnemyCharacter::FreezeDeathPose()
{
    if (HasAuthority())
    {
        Multicast_FreezeDeathPose();
    }
    else
    {
        Multicast_FreezeDeathPose_Implementation();
    }
}

void AEnemyCharacter::Multicast_FreezeDeathPose_Implementation()
{
    // 무브먼트 컴포넌트 틱까지 완전히 끄고 비활성화 (밀림 현상 0% 원천 차단)
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
        Move->Velocity = FVector::ZeroVector;
        Move->DisableMovement();
        Move->SetComponentTickEnabled(false);
        Move->SetAvoidanceEnabled(false);
        Move->bUseRVOAvoidance = false;
        Move->bEnablePhysicsInteraction = false;
    }

    // 캡슐: 물리 충돌을 QueryOnly로 제한하고, 오직 바닥만 지지
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
        Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        Capsule->CanCharacterStepUpOn = ECB_No;
    }

    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        // 0) 이 화면에서 사망 몽타주를 마지막 프레임으로 맞춘 뒤 고정.
        //    클라이언트는 몽타주를 늦게 받거나(지연) 피격 몽타주에 덮여 못 받은 상태에서 고정 명령이 오면
        //    서 있는 포즈 그대로 멈추던 문제 방지.
        if (DeathMontage)
        {
            if (UAnimInstance* AnimInst = Skel->GetAnimInstance())
            {
                const float EndPos = FMath::Max(0.f, DeathMontage->GetPlayLength() - 0.02f);
                if (!AnimInst->Montage_IsPlaying(DeathMontage))
                {
                    DeathMontage->bEnableAutoBlendOut = false;
                    // 블렌드 인 없이 즉시 100% 적용 (블렌드 중이면 서 있는 포즈가 섞여 남음)
                    AnimInst->Montage_PlayWithBlendIn(DeathMontage, FAlphaBlendArgs(0.f), 1.0f);
                }
                AnimInst->Montage_SetPosition(DeathMontage, EndPos);
                AnimInst->Montage_SetPlayRate(DeathMontage, 0.f); // 마지막 프레임 유지
            }
        }

        // 1) 포즈가 실제로 반영될 시간을 준 뒤(0.15초) 애니/틱 정지.
        //    바로 끄면 애니 평가 결과가 화면에 반영되기 전에 멈춰 서 있는 포즈로 남을 수 있음.
        {
            TWeakObjectPtr<USkeletalMeshComponent> WeakSkel(Skel);
            FTimerHandle PauseTimer;
            GetWorldTimerManager().SetTimer(PauseTimer, [WeakSkel]()
            {
                if (USkeletalMeshComponent* S = WeakSkel.Get())
                {
                    S->bPauseAnims = true;
                    S->SetComponentTickEnabled(false);
                }
            }, 0.15f, false);
        }
        Skel->SetSimulatePhysics(false);
        Skel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Skel->SetCollisionResponseToAllChannels(ECR_Ignore);
        Skel->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        Skel->CanCharacterStepUpOn = ECB_No;

        // 2) 시체 중심 위치 계산 (pelvis 기준, 없으면 Bounds 중심)
        if (InteractCollision)
        {
            // 스켈레톤에서 실제 골반 뼈 확인해서 바꿔줘
            static const FName PelvisBoneName(TEXT("pelvis"));

            FVector CorpseCenter;

            if (Skel->DoesSocketExist(PelvisBoneName))
            {
                CorpseCenter = Skel->GetSocketLocation(PelvisBoneName);
            }
            else
            {
                // 소켓 못 찾으면 메쉬 바운즈 중심 사용
                CorpseCenter = Skel->Bounds.Origin;
            }

            // 3) 콜리전을 그 위치로 옮기고, 루트에서 분리 (루트 움직여도 그대로 유지)
            InteractCollision->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
            InteractCollision->SetWorldLocation(CorpseCenter);
        }
    }

    // 4) 이 시점 이후에 루팅 가능하게
    EnableCorpseInteraction();

    // 5) 사망 애니메이션이 완전히 끝났으므로 안전하게 컨트롤러 조종(빙의) 해제
    if (HasAuthority())
    {
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            AIC->UnPossess();
        }
    }
}

UAnimMontage* AEnemyCharacter::GetHitMontage(FGameplayTag HitTag) const
{
    // 1. 매칭되는 히트 태그(Light, Heavy 등)를 찾음
    if (UAnimMontage* const* FoundMontage = HitMontages.Find(HitTag))
    {
        return *FoundMontage;
    }

    // 2. 부분 일치 탐색 (예: Effect.Hit.Light 요청 시 Effect.Hit.Light.Front 등 매칭)
    for (const auto& Pair : HitMontages)
    {
        if (Pair.Key.MatchesTag(HitTag) || HitTag.MatchesTag(Pair.Key))
        {
            if (Pair.Value) return Pair.Value;
        }
    }

    // 3. 디폴트(Light) 태그 탐색
    FGameplayTag DefaultTag = NonTags::Effect_Hit_Light.GetTag();
    if (UAnimMontage* const* DefaultMontage = HitMontages.Find(DefaultTag))
    {
        return *DefaultMontage;
    }

    // 4. 안전 Fallback: 맵에 등록된 첫 번째 유효한 몽타주 무조건 반환
    for (const auto& Pair : HitMontages)
    {
        if (Pair.Value) return Pair.Value;
    }

    return nullptr;
}

void AEnemyCharacter::SetInteractionOutline(bool bEnable)
{
    if (CorpseComponent) CorpseComponent->SetInteractionOutline(bEnable);
}

bool AEnemyCharacter::IsSpawnFading() const
{
    return CorpseComponent && CorpseComponent->IsSpawnFading();
}

void AEnemyCharacter::PlaySpawnFadeIn(float Duration)
{
    if (CorpseComponent) CorpseComponent->PlaySpawnFadeIn(Duration < 0.f ? SpawnFadeDuration : Duration);
}

void AEnemyCharacter::DistributeExpOnDeath(ANonCharacterBase* KillerChar)
{
    if (CorpseComponent) CorpseComponent->DistributeExp(KillerChar, ExpReward);
}

void AEnemyCharacter::EnableCorpseInteraction()
{
    if (CorpseComponent) CorpseComponent->EnableCorpseInteraction();
}

void AEnemyCharacter::Interact_Implementation(ANonCharacterBase* Interactor)
{
    if (CorpseComponent) CorpseComponent->HandleInteract(Interactor);
}

FText AEnemyCharacter::GetInteractLabel_Implementation()
{
    return CorpseComponent ? CorpseComponent->GetInteractLabel() : FText::GetEmpty();
}

void AEnemyCharacter::SetInteractHighlight_Implementation(bool bEnable)
{
    if (CorpseComponent) CorpseComponent->SetInteractHighlight(bEnable);
}

void AEnemyCharacter::ApplyHealthDelta_Direct(float Delta)
{
    if (!AbilitySystemComponent || !AttributeSet) return;

    const float Cur = AttributeSet->GetHP();
    const float Max = AttributeSet->GetMaxHP();
    const float NewVal = FMath::Clamp(Cur + Delta, 0.f, Max);

    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetHPAttribute(), NewVal);
    UpdateHPBar();
}

bool AEnemyCharacter::HasDeadTag() const
{
    if (!AbilitySystemComponent || DeadTagName.IsNone()) return false;
    const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(DeadTagName, false);
    return Tag.IsValid() ? AbilitySystemComponent->HasMatchingGameplayTag(Tag) : false;
}

void AEnemyCharacter::AddDeadTag()
{
    if (!AbilitySystemComponent || DeadTagName.IsNone()) return;
    const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(DeadTagName, false);
    if (Tag.IsValid())
    {
        AbilitySystemComponent->AddLooseGameplayTag(Tag);
    }
}

void AEnemyCharacter::ApplyDamage(float Amount, AActor* DamageInstigator)
{
    EnterCombat();
    ApplyDamageAt(Amount, DamageInstigator, GetActorLocation());
}

// [Updated] for Hit Reaction Tag
void AEnemyCharacter::ApplyDamageAt(float Amount, AActor* DamageInstigator, const FVector& WorldLocation, bool bIsCritical, FGameplayTag ReactionTag)
{
    if (!AbilitySystemComponent || !AttributeSet) return;
    if (IsDead() || bDied) return;

    UpdateCombatActionTime(); // 피격 시 전투 시간 갱신!

    // ── [New] 동일 공격자 0.25초 이내 중복 타격 방지 (멀티플레이 중복 판정 2중 방어) ──
    if (DamageInstigator)
    {
        const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
        if (float* LastHitTime = RecentAttackerHitTimes.Find(DamageInstigator))
        {
            if ((CurrentTime - *LastHitTime) < 0.25f)
            {
                UE_LOG(LogTemp, Verbose, TEXT("[HitFilter] %s dropped hit from %s (%.3fs since last, dmg %.1f)"),
                       *GetName(), *GetNameSafe(DamageInstigator), CurrentTime - *LastHitTime, Amount);
                return; // 0.25초 이내 동일 공격자의 연속 타격 무시
            }
        }
        RecentAttackerHitTimes.Add(DamageInstigator, CurrentTime);
    }

    // ── 무적 / 회피 태그 체크 시 데미지 무시 및 "무적" / "회피" 텍스트 팝업 ──
    static const FGameplayTag Tag_Invincible = NonTags::State_Invincible.GetTag();
    static const FGameplayTag Tag_IFrame = NonTags::State_IFrame.GetTag();
    if (AbilitySystemComponent->HasMatchingGameplayTag(Tag_Invincible))
    {
        Multicast_SpawnImmuneText(WorldLocation);
        return;
    }
    else if (AbilitySystemComponent->HasMatchingGameplayTag(Tag_IFrame))
    {
        Multicast_SpawnDodgeText(WorldLocation);
        return;
    }

    // 데미지도 없고 피격 태그도 없으면 무시
    if (Amount <= 0.f && !ReactionTag.IsValid()) return;

    //  마지막으로 나를 공격한 플레이어 기억 (경험치 지급용)
    if (DamageInstigator)
    {
        // 직접 플레이어일 때
        if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(DamageInstigator))
        {
            LastDamageInstigator = Player;
        }
        else if (APawn* InstPawn = Cast<APawn>(DamageInstigator))
        {
            if (ANonCharacterBase* PlayerFromPawn = Cast<ANonCharacterBase>(InstPawn))
            {
                LastDamageInstigator = PlayerFromPawn;
            }
        }
    }

    // ── 실제 HP 감소 (데미지가 0.1 초과일 때만 GAS GE_Damage 처리)
    if (Amount > 0.1f)
    {
        if (GE_Damage)
        {
            FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
            Ctx.AddInstigator(DamageInstigator, Cast<APawn>(DamageInstigator) ? Cast<APawn>(DamageInstigator)->GetController() : nullptr);

            FHitResult HitResult;
            HitResult.Location = WorldLocation;
            HitResult.ImpactPoint = WorldLocation;
            Ctx.AddHitResult(HitResult);

            FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(GE_Damage, 1.f, Ctx);
            if (Spec.IsValid())
            {
                const FGameplayTag Tag_Damage = NonTags::Data_Damage.GetTag();
                Spec.Data->SetSetByCallerMagnitude(Tag_Damage, -Amount);

                // [New] Critical 정보 전달 (AttributeSet에서 확인용)
                if (bIsCritical)
                {
                    Spec.Data->AddDynamicAssetTag(NonTags::Effect_Damage_Critical.GetTag());
                }

                // [New] Hit Reaction 정보 전달 (AttributeSet에서 Event trigger용)
                if (ReactionTag.IsValid())
                {
                    Spec.Data->AddDynamicAssetTag(ReactionTag);
                }

                AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
            }
        }
        else
        {
            ApplyHealthDelta_Direct(-Amount);
        }
    }
    else if (ReactionTag.IsValid())
    {
        // 💥 데미지가 0이어도 경직 태그(ReactionTag, 예: Effect.Hit.Light)가 있으면 GA_HitReaction 이벤트 직접 발송!
        FGameplayTag HitEventTag = ReactionTag;
        // Hit.Light 등의 형식으로 입력되었을 경우 Effect.Hit.Light 로 자동 매핑
        if (!HitEventTag.MatchesTag(NonTags::Effect_Hit.GetTag()))
        {
            FString TagStr = HitEventTag.ToString();
            if (TagStr.StartsWith(TEXT("Hit.")))
            {
                FGameplayTag MappedTag = FGameplayTag::RequestGameplayTag(*FString::Printf(TEXT("Effect.%s"), *TagStr), false);
                if (MappedTag.IsValid()) HitEventTag = MappedTag;
            }
        }

        FGameplayEventData Payload;
        Payload.EventTag = HitEventTag;
        Payload.Instigator = DamageInstigator;
        Payload.Target = this;
        Payload.EventMagnitude = 0.f;

        AbilitySystemComponent->HandleGameplayEvent(HitEventTag, &Payload);
    }

    // 피격 시 만약 경계 텀(대기) 중이었다면 즉시 대기를 취소하고 전투 돌입
    if (bIsAlerting)
    {
        if (AlertTimerHandle.IsValid())
        {
            GetWorldTimerManager().ClearTimer(AlertTimerHandle);
        }
        FinishAlertReaction();
    }

    // Reactive 어그로: "맞아서 어그로" 플래그 + 타임스탬프 기록
    if (AggroStyle == EAggroStyle::Reactive)
    {
        MarkAggroByHit(DamageInstigator);   // bAggroByHit = true, LastAggroByHitTime 업데이트
        SetAggro(true);
    }



    // 전투 상태 진입(HP바 표시 등 - 서버 로직)
    EnterCombat();

    // [New] 때린 플레이어에게 "체력바 띄워라" 알림 (Client RPC)
    if (DamageInstigator)
    {

        
        if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(DamageInstigator))
        {
            Player->ClientOnAttackHitEnemy(this);
        }
        else if (APawn* InstPawn = Cast<APawn>(DamageInstigator))
        {
            if (ANonCharacterBase* PlayerFromPawn = Cast<ANonCharacterBase>(InstPawn->GetController()->GetPawn())) // Controller Pawn check
            {
               PlayerFromPawn->ClientOnAttackHitEnemy(this);
            }
        }
    }
}

void AEnemyCharacter::MarkAggroByHit(AActor* InstigatorActor)
{
    // 누가 때렸는지까지 쓰고 싶으면 여기서 저장/검증 가능
    bAggroByHit = true;
    LastAggroByHitTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void AEnemyCharacter::Multicast_SpawnDamageNumber_Implementation(float Amount, FVector WorldLocation, bool bIsCritical)
{
    if (!GetWorld() || Amount <= 0.f) return;

    // ── [New] 원거리 데미지 숫자 가시거리 컬링 (25미터) ──
    if (APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0))
    {
        const float DistSq = FVector::DistSquared(CamMgr->GetCameraLocation(), WorldLocation);
        constexpr float MaxDamageDistSq = 2500.f * 2500.f; // 25미터 (2500cm)
        if (DistSq > MaxDamageDistSq)
        {
            return;
        }
    }

    TSubclassOf<ADamageNumberActor> SpawnClass = DamageNumberActorClass;
    if (!SpawnClass) SpawnClass = ADamageNumberActor::StaticClass();

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SP.Owner = this;

     if (ADamageNumberActor* A = GetWorld()->SpawnActor<ADamageNumberActor>(SpawnClass, WorldLocation + FVector(0,0,30), FRotator::ZeroRotator, SP))
    {
        A->InitWithFlags(Amount, bIsCritical);
    }
}

void AEnemyCharacter::Multicast_SpawnImmuneText_Implementation(FVector WorldLocation)
{
    if (!GetWorld()) return;

    // 원거리 가시거리 컬링 (25미터)
    if (APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0))
    {
        const float DistSq = FVector::DistSquared(CamMgr->GetCameraLocation(), WorldLocation);
        constexpr float MaxDamageDistSq = 2500.f * 2500.f;
        if (DistSq > MaxDamageDistSq) return;
    }

    TSubclassOf<ADamageNumberActor> SpawnClass = DamageNumberActorClass;
    if (!SpawnClass) SpawnClass = ADamageNumberActor::StaticClass();

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SP.Owner = this;

    FVector FinalLoc = WorldLocation;
    if (FinalLoc.IsNearlyZero())
    {
        FinalLoc = GetActorLocation() + FVector(0.f, 0.f, 50.f);
    }
    else
    {
        FinalLoc += FVector(0.f, 0.f, 30.f);
    }

    if (ADamageNumberActor* A = GetWorld()->SpawnActor<ADamageNumberActor>(SpawnClass, FinalLoc, FRotator::ZeroRotator, SP))
    {
        A->SetupAsImmune();
    }
}

void AEnemyCharacter::Multicast_SpawnDodgeText_Implementation(FVector WorldLocation)
{
    if (!GetWorld()) return;

    // 원거리 가시거리 컬링 (25미터)
    if (APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0))
    {
        const float DistSq = FVector::DistSquared(CamMgr->GetCameraLocation(), WorldLocation);
        constexpr float MaxDamageDistSq = 2500.f * 2500.f;
        if (DistSq > MaxDamageDistSq) return;
    }

    TSubclassOf<ADamageNumberActor> SpawnClass = DamageNumberActorClass;
    if (!SpawnClass) SpawnClass = ADamageNumberActor::StaticClass();

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SP.Owner = this;

    FVector FinalLoc = WorldLocation;
    if (FinalLoc.IsNearlyZero())
    {
        FinalLoc = GetActorLocation() + FVector(0.f, 0.f, 50.f);
    }
    else
    {
        FinalLoc += FVector(0.f, 0.f, 30.f);
    }

    if (ADamageNumberActor* A = GetWorld()->SpawnActor<ADamageNumberActor>(SpawnClass, FinalLoc, FRotator::ZeroRotator, SP))
    {
        A->SetupAsDodge();
    }
}

void AEnemyCharacter::UpdateCombatActionTime()
{
    if (UWorld* World = GetWorld())
    {
        LastCombatActionTime = World->GetTimeSeconds();
    }
}

void AEnemyCharacter::EnterCombat()
{
    bInCombat = true;
    UpdateCombatActionTime();
    UpdateHPBarVisibility();

    GetWorldTimerManager().ClearTimer(CombatTimeoutTimer);
    GetWorldTimerManager().SetTimer(CombatTimeoutTimer, this, &AEnemyCharacter::LeaveCombat, CombatTimeout, false);
}

void AEnemyCharacter::LeaveCombat()
{
    bInCombat = false;
    UpdateHPBarVisibility();
}

// [Removed] Duplicate UpdateHPBar definition (It is already defined earlier in the file)

void AEnemyCharacter::UpdateHPBarVisibility()
{
    if (!HPBarWidget) return;

    if (IsDead())
    {
        HPBarWidget->SetVisibility(false);
        return;
    }

    // HP바 표시 조건 (화면을 보는 플레이어 기준):
    // 1) "전투 중에만 표시(bShowHPBarOnlyInCombat)" 옵션이 꺼져 있으면 상시 표시
    // 2) 켜져 있다면: 이 적이 내(로컬 플레이어)를 노리는 중이거나, 내가 최근에 적중시켰을 때(bClientHPBarVisible, 5초)만 표시.
    //    다른 플레이어를 쫓는 적, 어그로가 풀린 적은 HP가 깎여 있어도 숨김.
    bool bShouldShow = false;
    if (!bShowHPBarOnlyInCombat)
    {
        bShouldShow = true;
    }
    else
    {
        bShouldShow = IsTargetingLocalPlayer() || bClientHPBarVisible;
    }

    HPBarWidget->SetVisibility(bShouldShow);

    if (bShouldShow)
    {
        UpdateHPBar(); // 최신 수치 갱신
    }
}

void AEnemyCharacter::ShowLocalHPBar(float Duration)
{
    bClientHPBarVisible = true;
    UpdateHPBarVisibility();

    // 타이머 갱신
    GetWorldTimerManager().ClearTimer(ClientHPBarTimer);
    GetWorldTimerManager().SetTimer(ClientHPBarTimer, this, &AEnemyCharacter::HideLocalHPBar, Duration, false);
}

void AEnemyCharacter::HideLocalHPBar()
{
    bClientHPBarVisible = false;
    UpdateHPBarVisibility();
}

void AEnemyCharacter::SetAggroTarget(AActor* NewTarget)
{
    if (!HasAuthority() || AggroTarget == NewTarget)
    {
        return;
    }

    AActor* OldTarget = AggroTarget;
    AggroTarget = NewTarget;
    // 서버(리슨 서버 방장/싱글)는 OnRep이 오지 않으므로 직접 처리
    HandleAggroTargetChanged(OldTarget, NewTarget);
}

void AEnemyCharacter::OnRep_AggroTarget(AActor* OldAggroTarget)
{
    HandleAggroTargetChanged(OldAggroTarget, AggroTarget);
}

void AEnemyCharacter::HandleAggroTargetChanged(AActor* OldTarget, AActor* NewTarget)
{
    if (ANonCharacterBase* OldPlayer = Cast<ANonCharacterBase>(OldTarget))
    {
        OldPlayer->NotifyEnemyAggroChanged(this, false);
    }
    if (ANonCharacterBase* NewPlayer = Cast<ANonCharacterBase>(NewTarget))
    {
        NewPlayer->NotifyEnemyAggroChanged(this, true);
    }
    UpdateHPBarVisibility();
}

bool AEnemyCharacter::IsTargetingLocalPlayer() const
{
    const APawn* TargetPawn = Cast<APawn>(AggroTarget);
    return TargetPawn && TargetPawn->IsLocallyControlled();
}

void AEnemyCharacter::SetAggro(bool bNewAggro)
{
    if (bAggro == bNewAggro)
    {
        return;
    }

    bAggro = bNewAggro;

    if (bAggro)
    {
        UpdateCombatActionTime(); // 어그로 획득 시 전투 시간 즉시 갱신!
    }
    else
    {
        LastCombatActionTime = -1.0f; // 어그로 해제 시 이전 전투 시간 리셋!

        // 어그로 해제 시 경계 타이머/상태도 초기화
        if (AlertTimerHandle.IsValid())
        {
            GetWorldTimerManager().ClearTimer(AlertTimerHandle);
        }
        bIsAlerting = false;
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            AIC->ClearFocus(EAIFocusPriority::Gameplay);
        }
    }

    // 어그로 상태가 바뀌면 HP바도 즉시 갱신
    UpdateHPBarVisibility();
}

void AEnemyCharacter::TriggerAlertReaction(AActor* InTarget)
{
    if (bIsAlerting || !InTarget || IsDead()) return;

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    if ((Now - LastAlertTime) < 12.0f)
    {
        return; // 최근 12초 이내에 이미 경계 반응을 거쳤다면 0.8초 텀 없이 즉시 반응
    }
    LastAlertTime = Now;

    bIsAlerting = true;
    UpdateCombatActionTime(); // 경계 반응 시작 시 전투 시간 갱신!

    // 1) 즉시 이동만 정지 (몸을 휙 돌리지 않고, 가던 방향/바라보던 방향 그대로 멈춰서 경계)
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        AIC->StopMovement();
        AIC->ClearFocus(EAIFocusPriority::Gameplay);
    }
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
        Move->bOrientRotationToMovement = false;
        Move->MaxWalkSpeed = 0.0f;
    }

    // 2) 몽타주가 있다면 재생
    float Duration = AlertDelay;
    if (AlertMontage)
    {
        const float MontageLen = PlayAnimMontage(AlertMontage);
        if (MontageLen > 0.0f)
        {
            Duration = FMath::Max(AlertDelay, MontageLen);
        }
    }

    // 4) 머리 위 경계 효과(AlertWidget) 자동 표시 (멀티캐스트로 모든 클라이언트에 브로드캐스트)
    if (bShowAlertMark)
    {
        const float WidgetShowTime = (Duration > 0.1f) ? FMath::Min(Duration, 0.7f) : 0.6f;
        if (HasAuthority())
        {
            Multicast_ShowAlertMark(WidgetShowTime);
        }
        else
        {
            Multicast_ShowAlertMark_Implementation(WidgetShowTime);
        }
    }

    // 5) 블루프린트 이벤트 방송 (추가 사운드나 특수 연출 필요시 BP에서도 수신 가능)
    OnEnemyAlerted.Broadcast(InTarget);



    // 6) 대기 텀 후 정상 추격 돌입 타이머 설정
    if (Duration > 0.0f)
    {
        GetWorldTimerManager().SetTimer(AlertTimerHandle, this, &AEnemyCharacter::FinishAlertReaction, Duration, false);
    }
    else
    {
        FinishAlertReaction();
    }
}

void AEnemyCharacter::Multicast_ShowAlertMark_Implementation(float ShowDuration)
{
    if (!AlertWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[경계 디버그] %s | AlertWidget 컴포넌트가 null입니다!"), *GetName());
        return;
    }

    UClass* WClass = AlertWidget->GetWidgetClass();
    if (!WClass)
    {
        const FString Msg = FString::Printf(TEXT("[경계 디버그] %s | AlertWidget에 WidgetClass(WBP_AlertMark)가 지정되지 않았습니다! BP_Enemy_Dummy의 AlertWidget 디테일에서 Widget Class를 확인하세요."), *GetName());
        if (GEngine) GEngine->AddOnScreenDebugMessage((uint64)this + 888, 3.0f, FColor::Red, Msg);
        UE_LOG(LogTemp, Error, TEXT("%s"), *Msg);
        return;
    }

    // 위젯이 생성되지 않았으면 즉시 초기화
    if (!AlertWidget->GetUserWidgetObject())
    {
        AlertWidget->InitWidget();
    }

    // ★ [카메라 시점 기준 우측 상단 배치]
    // 몬스터의 몸체 회전에 영향을 받지 않도록, Pivot을 (0.0, 1.2)로 주어
    // 화면상에서 머리 중앙을 기준으로 오른쪽(X: 0.0 -> 위젯 중심이 오른쪽으로 이동) + 위쪽(Y: 1.2 -> 머리 위로 이동)에 자연스럽게 위치시킵니다.
    AlertWidget->SetPivot(FVector2D(0.0f, 1.1f));

    AlertWidget->SetVisibility(true);

    GetWorldTimerManager().ClearTimer(AlertWidgetTimerHandle);
    if (ShowDuration > 0.0f)
    {
        GetWorldTimerManager().SetTimer(AlertWidgetTimerHandle, this, &AEnemyCharacter::HideAlertMark, ShowDuration, false);
    }
}

void AEnemyCharacter::Multicast_HideAlertMark_Implementation()
{
    if (AlertWidget)
    {
        AlertWidget->SetVisibility(false);
    }
    if (AlertWidgetTimerHandle.IsValid())
    {
        GetWorldTimerManager().ClearTimer(AlertWidgetTimerHandle);
    }
}

void AEnemyCharacter::HideAlertMark()
{
    if (HasAuthority())
    {
        Multicast_HideAlertMark();
    }
    else
    {
        Multicast_HideAlertMark_Implementation();
    }
}

void AEnemyCharacter::FinishAlertReaction()
{
    if (!bIsAlerting) return;

    bIsAlerting = false;
    UpdateCombatActionTime(); // 본격 추적 돌입 시 전투 시간 갱신!

    // 만약 아직 위젯이 켜져있다면 끄기
    HideAlertMark();

    // AI Focus 해제 (자연스러운 이동 회전으로 복귀)
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        AIC->ClearFocus(EAIFocusPriority::Gameplay);
    }

    // [3단계: 플레이어 접근 개시] 어그로 확정 및 달리기 속도로 회복
    SetAggro(true);

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->bOrientRotationToMovement = true;
        Move->MaxWalkSpeed = (CombatRunSpeed > 0.0f) ? CombatRunSpeed : 400.0f;
    }
}

void AEnemyCharacter::TryStartAttack()
{
    EnterCombat(); // 전투 상태 갱신

    if (!IsAttackAllowed()) 
    {
        return;
    }

    // 타겟(플레이어)을 정면으로 바라보도록 즉시 회전 보정
    AActor* TargetActor = nullptr;
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
        {
            TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
        }
    }
    // 블랙보드에 대상이 없으면 복제되는 추격 대상으로 대체 (첫 플레이어로 대체하면 협동에서 항상 호스트를 노림)
    if (!TargetActor)
    {
        TargetActor = GetAggroTarget();
    }

    if (TargetActor)
    {
        FVector Forward = GetActorForwardVector().GetSafeNormal2D();
        FVector DirToTarget = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
        if (!DirToTarget.IsNearlyZero())
        {
            float Dot = FVector::DotProduct(Forward, DirToTarget);
            float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
            // 정면 시야각(75도 이내)일 때만 정밀 조준 보정 (등 뒤에 있을 때 180도 홱 도는 현상 방지)
            if (AngleDeg <= 75.0f)
            {
                FRotator TargetRot = DirToTarget.Rotation();
                TargetRot.Pitch = 0.0f;
                TargetRot.Roll = 0.0f;
                SetActorRotation(TargetRot);
            }
        }
    }

    if (AbilitySystemComponent)
    {
        // GAS 기반 공격 시도 (Enemy.Skill.Melee 및 Ability.Attack 둘 다 시도 가능)
        FGameplayTagContainer ActivateTags; 
        const FGameplayTag MeleeTag = NonTags::Enemy_Skill_Melee.GetTag();
        if (MeleeTag.IsValid())
        {
            ActivateTags.AddTag(MeleeTag);
        }
        const FGameplayTag AttackTag = NonTags::Ability_Attack.GetTag();
        if (AttackTag.IsValid())
        {
            ActivateTags.AddTag(AttackTag);
        }

        AbilitySystemComponent->TryActivateAbilitiesByTag(ActivateTags);
    }
}

// [Legacy Attack Functions Removed] - PickAttackMontage, PlayAttackMontage, Multicast_PlayAttack
// Replaced by GA_EnemyAttack logic.

// [Legacy] PlayMontageSafe Removed

void AEnemyCharacter::AttackHitbox_Enable()
{
    bIsAttackHitboxActive = true;
    HitOnce.Reset();

    for (UShapeComponent* Hitbox : ActiveHitboxes)
    {
        if (Hitbox)
        {
            Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Hitbox->SetGenerateOverlapEvents(true);
        }
    }
}

void AEnemyCharacter::AttackHitbox_Disable()
{
    bIsAttackHitboxActive = false;

    for (UShapeComponent* Hitbox : ActiveHitboxes)
    {
        if (Hitbox)
        {
            Hitbox->SetGenerateOverlapEvents(false);
            Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }

    HitOnce.Reset();
}

void AEnemyCharacter::OnAttackHitBegin(UPrimitiveComponent* Overlapped, AActor* Other,
    UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
    // 서버에서만 데미지/전투상태 처리
    if (!HasAuthority()) return;

    if (!Other || Other == this) return;
    if (HitOnce.Contains(Other)) return;

    // ★ 다른 몬스터나 보스(시체 포함)는 공격 대상에서 제외!
    if (Other->IsA<AEnemyCharacter>()) return;

    // 플레이어만 맞추기 (이미 죽은 대상 제외)
    if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(Other))
    {
        if (Player->IsDead()) return;

        HitOnce.Add(Other);

        // 여기서 플레이어 전투 상태 진입
        Player->EnterCombatState();

        // 1) 기본 맞은 위치 후보
        FVector HitLoc = Sweep.ImpactPoint;

        // 2) Overlap이면 ImpactPoint가 (0,0,0)일 수 있음 → 타겟 충돌체에서 "가장 가까운 점" 보정
        if (HitLoc.IsNearlyZero())
        {
            FVector Closest;
            const FVector From = Overlapped ? Overlapped->GetComponentLocation() : GetActorLocation();

            if (OtherComp && OtherComp->GetClosestPointOnCollision(From, /*out*/Closest))
            {
                HitLoc = Closest;
            }
            else
            {
                // 보정 실패시 타겟 머리/상체나 액터 위치 근처로 안전 보정
                if (USkeletalMeshComponent* Skel = Player->GetMesh())
                {
                    const FName PrefSockets[] = { TEXT("head"), TEXT("spine_03"), TEXT("spine_02"), TEXT("spine_01") };
                    bool bDone = false;
                    for (const FName& S : PrefSockets)
                    {
                        if (Skel->DoesSocketExist(S)) { HitLoc = Skel->GetSocketLocation(S); bDone = true; break; }
                    }
                    if (!bDone) HitLoc = Player->GetActorLocation() + FVector(0,0,90.f);
                }
                else
                {
                    HitLoc = Player->GetActorLocation() + FVector(0,0,90.f);
                }
            }
        }
        // 3) 데미지 계산: 적 스탯 → 원 데미지 → 플레이어 방어/마저 반영
        const float RawDamage = UNonDamageHelpers::ComputeDamageFromAttributes(
            this,               // 공격자: 적
            AttackPowerScale,   // 스킬 계수 (BP에서 튜닝)
            AttackDamageType);  // 물리/마법

        if (RawDamage > 0.f)
        {
            const float FinalDamage = UNonDamageHelpers::ApplyDefenseReduction(
                Player, RawDamage, AttackDamageType
            );

            if (FinalDamage > 0.f)
            {
                const FGameplayTag AppliedReactionTag = CurrentAttackHitReactionTag.IsValid()
                    ? CurrentAttackHitReactionTag
                    : NonTags::Effect_Hit_Light.GetTag();

                Player->ApplyDamageAt(FinalDamage, this, HitLoc, AppliedReactionTag);
                UpdateCombatActionTime(); // 공격 성공 시 전투 무반응 타이머 갱신
            }
        }
    }
}

void AEnemyCharacter::TickCombatRotation(float DeltaSeconds)
{
    // 1) 사망 상태면 회전 금지
    if (IsDead())
    {
        return;
    }

    // 현재 재생 중인 몽타주가 피격(HitReaction) 몽타주인지 판별
    UAnimMontage* CurMontage = GetCurrentMontage();
    bool bIsPlayingHitMontage = false;
    if (CurMontage)
    {
        for (const auto& Pair : HitMontages)
        {
            if (Pair.Value == CurMontage)
            {
                bIsPlayingHitMontage = true;
                break;
            }
        }
    }

    if (AbilitySystemComponent)
    {
        static const FGameplayTag StateHitReactTag = NonTags::State_HitReacting.GetTag();
        if (StateHitReactTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(StateHitReactTag))
        {
            bIsPlayingHitMontage = true;
        }
    }

    // 타겟 액터 찾기 (보스는 복제된 CurrentTargetActor 우선 참조하여 서버/클라이언트 완벽 일치 보장)
    AActor* TargetActor = nullptr;
    if (const ABossCharacter* Boss = Cast<ABossCharacter>(this))
    {
        TargetActor = Boss->CurrentTargetActor.Get();
    }
    // 추격 대상(AggroTarget/블랙보드 TargetActor)을 마지막 공격자보다 우선한다.
    // 예전에는 LastDamageInstigator가 먼저여서, B를 추격 중일 때 A가 때리면 A를 바라보며 굳어
    // B를 공격할 각도가 나오지 않았음. (AggroTarget은 복제되므로 클라이언트에서도 같은 대상)
    if (!TargetActor)
    {
        TargetActor = AggroTarget.Get();
    }
    if (!TargetActor)
    {
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
            {
                TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
            }
        }
    }
    if (!TargetActor)
    {
        TargetActor = LastDamageInstigator.Get();
    }

    // ★ [핵심: 피격(HitReaction) 몽타주 재생 중 캡슐 정면 강제 고정 (단, 보스는 피격/부위파괴 중 플레이어를 따라 돌지 않도록 완전 고정!)]
    if (bIsPlayingHitMontage)
    {
        if (!IsBoss() && TargetActor)
        {
            const FVector DirToTarget = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
            if (!DirToTarget.IsNearlyZero())
            {
                FRotator LockRot = DirToTarget.Rotation();
                LockRot.Pitch = 0.0f;
                LockRot.Roll = 0.0f;
                SetActorRotation(LockRot);
            }
        }
        return;
    }

    // 2) 공격 몽타주 등 다른 몽타주가 재생 중일 때의 처리:
    // ★ [AAA 액션 게임 표준 공격 선딜레이 유도 회전 (Attack Windup Tracking)]
    // - 제자리 턴(Turn), 사망(Death), 경계(Alert), 백스텝(Backstep), 페이즈 전환, 부위 파괴(Pain) 중에는 수동 회전 완전 차단!
    // - 공격/스킬 몽타주의 경우:
    //   ① 선딜레이(무기/주먹을 들어 올리는 준비 동작, ANS_AttackHitboxWindow 시작 전) 구간에는 AttackWindupTurnRate(기본 초당 75도)로 천천히 플레이어를 따라 회전!
    //   ② 실제 타격 판정 구간(ANS_AttackHitboxWindow 활성 중) 및 마지막 타격 이후 후딜레이(Recovery) 구간에는 회전을 100% 고정(Lock)!
    if (CurMontage != nullptr)
    {
        const ABossCharacter* Boss = Cast<ABossCharacter>(this);
        if (Boss && (Boss->bIsTransitioningPhase || Boss->bIsPlayingPartBreak || Boss->bIsGroggy || Boss->IsPlayingPartBreakMontage(CurMontage)))
        {
            return;
        }

        if (CurMontage == TurnLeftMontage || CurMontage == TurnRightMontage ||
            CurMontage == DeathMontage || CurMontage == AlertMontage || CurMontage == BackstepMontage)
        {
            return;
        }

        if (AbilitySystemComponent)
        {
            static const FGameplayTag StateTurnTag = NonTags::State_Turn.GetTag();
            static const FGameplayTag StateStunnedTag = NonTags::State_Stunned.GetTag();
            static const FGameplayTag StateHitReactingTag = NonTags::State_HitReacting.GetTag();
            if ((StateTurnTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(StateTurnTag)) ||
                (StateStunnedTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(StateStunnedTag)) ||
                (StateHitReactingTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(StateHitReactingTag)))
            {
                return;
            }
        }

        // ★ [후방 공격(Back Attack) 회전 완전 차단]
        // 보스/몬스터가 등 뒤의 플레이어를 기습하는 Back Attack(후방 공격/꼬리치기/후방치기 등) 몽타주를 재생할 때는,
        // 플레이어가 등 뒤에 있다고 해서 플레이어 쪽으로 몸을 180도 돌려버리면 후방 공격의 의미가 사라지므로 회전을 100% 완전 고정!
        const bool bIsBackAttack = IsBackAttackMontage(CurMontage);

        if (bIsBackAttack)
        {
            return;
        }

        if (bAllowAttackWindupTracking && AttackWindupTurnRate > 0.0f && TargetActor && !bIsAttackHitboxActive)
        {
            UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
            const float CurPos = AnimInst ? AnimInst->Montage_GetPosition(CurMontage) : 0.0f;

            bool bInMotionWarpingWindow = false;
            bool bInHitboxWindow = false;
            bool bHasHitboxNotify = false;
            float LastHitboxStartTime = 0.0f;

            for (const FAnimNotifyEvent& NotifyEvent : CurMontage->Notifies)
            {
                const float StartTime = NotifyEvent.GetTriggerTime();
                const float EndTime = NotifyEvent.GetEndTriggerTime();

                if (NotifyEvent.NotifyStateClass)
                {
                    if (NotifyEvent.NotifyStateClass->IsA<UAnimNotifyState_MotionWarping>())
                    {
                        if (CurPos >= StartTime && CurPos <= EndTime)
                        {
                            bInMotionWarpingWindow = true;
                        }
                    }
                    else if (NotifyEvent.NotifyStateClass->IsA<UANS_AttackHitboxWindow>())
                    {
                        bHasHitboxNotify = true;
                        LastHitboxStartTime = FMath::Max(LastHitboxStartTime, StartTime);
                        if (CurPos >= StartTime && CurPos <= EndTime)
                        {
                            bInHitboxWindow = true;
                        }
                    }
                }
                else if (NotifyEvent.Notify)
                {
                    // ★ 보스/몬스터가 Spawn Actor (UAnimNotify_SpawnActor)로 데미지 장판·투사체를 소환하는 공격도
                    // 액터가 소환되기 직전(선딜레이)까지 플레이어를 향해 부드럽게 유도 회전하고, 소환 순간부터 후딜레이까지는 100% 고정!
                    if (NotifyEvent.Notify->IsA<UAnimNotify_SpawnActor>())
                    {
                        bHasHitboxNotify = true;
                        LastHitboxStartTime = FMath::Max(LastHitboxStartTime, StartTime);
                    }
                }
            }

            bool bCanTrackDuringWindup = false;
            if (!bInMotionWarpingWindow && !bInHitboxWindow)
            {
                if (bHasHitboxNotify)
                {
                    // 마지막 타격 판정(또는 SpawnActor 노티파이)이 시작되기 전(선딜레이 구간)에만 천천히 유도 회전 허용!
                    // 타격/소환 순간부터 후딜레이 끝까지는 회전을 100% 고정!
                    bCanTrackDuringWindup = (CurPos < LastHitboxStartTime);
                }
                else
                {
                    bool bIsRegisteredAttackMontage = AttackMontages.Contains(CurMontage) || (CurMontage == RangeAttackMontage);
                    if (!bIsRegisteredAttackMontage && Boss && Boss->BossData)
                    {
                        for (const FBossPhaseData& PhaseData : Boss->BossData->PhaseList)
                        {
                            for (const FBossSkillEntry& SkillEntry : PhaseData.Skills)
                            {
                                if (SkillEntry.Montage == CurMontage)
                                {
                                    bIsRegisteredAttackMontage = true;
                                    break;
                                }
                            }
                            if (bIsRegisteredAttackMontage) break;
                        }
                    }

                    if (bIsRegisteredAttackMontage)
                    {
                        // 노티파이가 별도로 없는 공격 몽타주는 초반 35% 선딜레이 동안 유도 회전
                        const float MontageLen = CurMontage->GetPlayLength();
                        bCanTrackDuringWindup = (MontageLen > 0.0f && CurPos < (MontageLen * 0.35f));
                    }
                }
            }

            if (bCanTrackDuringWindup)
            {
                const FVector DirToTarget = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
                if (!DirToTarget.IsNearlyZero())
                {
                    FRotator TargetRot = DirToTarget.Rotation();
                    TargetRot.Pitch = 0.0f;
                    TargetRot.Roll = 0.0f;

                    const FRotator CurrentRot = GetActorRotation();
                    const float AngleDiff = FMath::Abs(FRotator::NormalizeAxis(TargetRot.Yaw - CurrentRot.Yaw));
                    if (AngleDiff > 2.0f)
                    {
                        const FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, TargetRot, DeltaSeconds, AttackWindupTurnRate);
                        SetActorRotation(NewRot);
                    }
                }
            }
        }
        return;
    }

    // ★ 보스이거나 전용 회전 몽타주(TurnLeftMontage / TurnRightMontage)가 등록된 몬스터는
    // 매 틱마다 미끄러지듯 강제 회전(SetActorRotation)하지 않고 비헤이비어 트리의 GA_EnemyTurn(Turn In Place 애니메이션)으로만 회전하도록 보호!
    if (IsBoss() || TurnLeftMontage != nullptr || TurnRightMontage != nullptr)
    {
        return;
    }

    // 3) GAS 태그(공격 중 / 회전 중) 검사하여 회전 완전 차단
    if (AbilitySystemComponent)
    {
        static const FGameplayTag StateAttackingTag = NonTags::State_Attacking.GetTag();
        static const FGameplayTag StateTurnTag = NonTags::State_Turn.GetTag();
        if ((StateAttackingTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(StateAttackingTag)) ||
            (StateTurnTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(StateTurnTag)))
        {
            return;
        }
    }

    // 4) 이미 이동 중이면 CharacterMovement의 bOrientRotationToMovement가 알아서 회전하므로 패스
    if (!GetVelocity().IsNearlyZero(10.f))
    {
        return;
    }

    if (!TargetActor)
    {
        return;
    }

    // 어그로 상태일 때만 회전 (비전투 순찰 상태에서 벽 너머 플레이어를 쳐다보는 것 방지)
    if (!bAggro && EnterRangeTime < 0.f)
    {
        return;
    }

    FVector DirToTarget = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    if (DirToTarget.IsNearlyZero())
    {
        return;
    }

    FRotator TargetRot = DirToTarget.Rotation();
    TargetRot.Pitch = 0.0f;
    TargetRot.Roll = 0.0f;

    FRotator CurrentRot = GetActorRotation();
    float AngleDiff = FMath::Abs(FRotator::NormalizeAxis(TargetRot.Yaw - CurrentRot.Yaw));

    // 각도 차이가 5도 이상일 때만 부드럽게 제자리 회전
    if (AngleDiff > 5.0f)
    {
        float TurnRate = 360.0f;
        if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
        {
            TurnRate = FMath::Max(MoveComp->RotationRate.Yaw, 360.0f);
        }

        FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, TargetRot, DeltaSeconds, TurnRate);
        SetActorRotation(NewRot);
    }
}

// ── 경직(이동 정지) ── [Legacy logic removed] - GA_HitReaction handles this if needed (Tags/Tasks)
// void AEnemyCharacter::StartHitMovePause(float OverrideDuration, bool bForceStop) { ... }
// void AEnemyCharacter::EndHitMovePause() { ... }

void AEnemyCharacter::BlockAttackFor(float Seconds)
{
    if (Seconds <= 0.f) return;
    const float Now = GetWorld()->GetTimeSeconds();
    NextAttackAllowedTime = Now + Seconds;
    NextMeleeAttackAllowedTime = FMath::Max(NextMeleeAttackAllowedTime, NextAttackAllowedTime);
    NextRangeAttackAllowedTime = FMath::Max(NextRangeAttackAllowedTime, NextAttackAllowedTime);
}

bool AEnemyCharacter::IsAttackAllowed() const
{
    return IsMeleeAttackAllowed();
}

bool AEnemyCharacter::IsMeleeAttackAllowed() const
{
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    return (Now >= NextMeleeAttackAllowedTime);
}

bool AEnemyCharacter::IsRangeAttackAllowed() const
{
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    return (Now >= NextRangeAttackAllowedTime);
}

void AEnemyCharacter::TriggerMeleeAttackCooldown()
{
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    NextMeleeAttackAllowedTime = Now + AttackCooldown;
    NextAttackAllowedTime = NextMeleeAttackAllowedTime;
}

void AEnemyCharacter::TriggerRangeAttackCooldown()
{
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    NextRangeAttackAllowedTime = Now + RangeAttackCooldown;
}


// (Windup) 관련 로직은 BT의 Cooldown/Wait 사용하므로 제거됨
void AEnemyCharacter::MarkEnteredAttackRange()
{
    if (EnterRangeTime < 0.f)
    {
        EnterRangeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
        // 진입 할 때마다 랜덤 딜레이 결정 (DT_EnemyData의 FirstAttackWindupMin ~ FirstAttackWindupMax)
        const float MinW = FMath::Max(0.0f, FirstAttackWindupMin);
        const float MaxW = FMath::Max(MinW, FirstAttackWindupMax);
        CurrentWindupTime = FMath::RandRange(MinW, MaxW);
    }
}

bool AEnemyCharacter::IsFirstAttackWindupDone() const
{
    // 뜸들이기 설정이 0초 이하(없음)인 경우 즉시 완료로 처리
    if (FirstAttackWindupMax <= 0.001f) return true;

    // 사정거리 진입 마킹 전이면 아직 완료되지 않음
    if (EnterRangeTime < 0.f) return false;

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    return (Now - EnterRangeTime) >= CurrentWindupTime;
}

void AEnemyCharacter::BindASCDelegates()
{
    if (AbilitySystemComponent)
    {
        FGameplayTag StunTag = NonTags::State_Stunned.GetTag();
        if (StunTag.IsValid())
        {
            AbilitySystemComponent->RegisterGameplayTagEvent(StunTag, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
        }
    }
}

float AEnemyCharacter::GetAngleToTarget(const AActor* TargetActor) const
{
    if (!TargetActor) return 0.0f;

    // 1. 적에서 타겟(플레이어)으로 향하는 방향 벡터 (Z축 무시)
    FVector DirectionToTarget = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();

    // 2. 적의 현재 정면 방향 벡터
    FVector ForwardVector = GetActorForwardVector().GetSafeNormal2D();

    // 3. 내적(Dot Product)을 통해 각도 크기 계산
    float Dot = FVector::DotProduct(ForwardVector, DirectionToTarget);
    float Angle = FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)) * (180.0f / UE_PI);

    // 4. 외적(Cross Product)으로 좌/우 구분 (+: 오른쪽, -: 왼쪽)
    FVector Cross = FVector::CrossProduct(ForwardVector, DirectionToTarget);
    if (Cross.Z < 0.0f)
    {
        Angle = -Angle;
    }

    return Angle;
}

bool AEnemyCharacter::TryPerformBackstepOrFlee(AActor* TargetActor)
{
    if (!TargetActor || IsDead()) return false;

    // 이미 후퇴 중인지 검사 (IsFleeing이 만료 검사 및 bIsFleeing 해제를 함께 수행)
    if (IsFleeing()) return false;

    UWorld* World = GetWorld();
    if (!World) return false;

    const float Now = World->GetTimeSeconds();
    if ((Now - LastFleeTime) < FleeCooldown)
    {
        return false; // 아직 쿨다운 중
    }

    // 어빌리티나 공격 몽타주가 재생 중인지 확인
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        if (ASC->HasMatchingGameplayTag(NonTags::State_Attacking.GetTag()))
        {
            return false; // 공격 중에는 백스텝 시도 안 함
        }
    }

    const FVector MyLoc = GetActorLocation();
    const FVector TargetLoc = TargetActor->GetActorLocation();
    const float Dist = FVector::Dist2D(MyLoc, TargetLoc);

    if (!bPendingHybridFlee && Dist > FleeTriggerDistance)
    {
        return false; // 아직 위험 거리가 아님
    }

    // 타겟의 반대 방향(후방) 벡터 계산
    FVector FleeDir = (MyLoc - TargetLoc).GetSafeNormal2D();
    if (FleeDir.IsNearlyZero())
    {
        FleeDir = -GetActorForwardVector().GetSafeNormal2D();
    }

    // 1) 백스텝 몽타주가 등록되어 있는 경우 -> 몽타주 재생 + 백스텝 도약!
    if (BackstepMontage)
    {
        // 타겟을 정면으로 바라보도록 회전 정렬
        FRotator FaceRot = (-FleeDir).Rotation();
        FaceRot.Pitch = 0.f;
        FaceRot.Roll = 0.f;
        SetActorRotation(FaceRot);

        const float MontageLen = PlayAnimMontage(BackstepMontage);
        const float Duration = (MontageLen > 0.f) ? MontageLen : 0.8f;

        // 뒤쪽 방향으로 부드럽게 백스텝 충격량(Launch) 부여
        const FVector LaunchVel = FleeDir * 650.0f + FVector(0.f, 0.f, 150.0f);
        LaunchCharacter(LaunchVel, true, true);

        bIsFleeing = true;
        bPendingHybridFlee = false;
        FleeEndTime = Now + Duration;
        LastFleeTime = Now;
        EnterRangeTime = -1.f; // 후퇴 완료 후 0.5초 조준 뜸들이기(Aim Delay) 자동 유도!
        return true;
    }

    // 2) 백스텝 몽타주가 없는 경우 -> 내비메시 상의 안전한 후방 위치로 후퇴(카이팅) 이동
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        // ★ [스폰 리쉬 구역 이탈 방지 스마트 카이팅]
        // 직선 후방으로 물러날 때 스폰 지점으로부터 너무 멀어지려 하면,
        // 스폰 지점 쪽(홈 구역)이나 측면으로 방향을 틀어 안전한 교전 영역 안에서 카이팅하도록 유도!
        const float DistFromHome = FVector::Dist2D(MyLoc, SpawnLocation);
        FVector AdjustedFleeDir = FleeDir;
        if (DistFromHome > 3000.0f)
        {
            FVector DirToHome = (SpawnLocation - MyLoc).GetSafeNormal2D();
            AdjustedFleeDir = (FleeDir * 0.4f + DirToHome * 0.6f).GetSafeNormal2D();
        }

        const float FleeDist = FMath::Clamp((FleeTriggerDistance - Dist) + 250.0f, 450.0f, 600.0f);
        FVector DesiredLoc = MyLoc + (AdjustedFleeDir * FleeDist);
        FNavLocation ProjectedNavLoc;
        UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

        bool bFoundNav = false;
        if (NavSys)
        {
            // 후방 목표 지점이 내비메시 위인지 검사
            bFoundNav = NavSys->ProjectPointToNavigation(DesiredLoc, ProjectedNavLoc, FVector(150.f, 150.f, 250.f));
            if (!bFoundNav)
            {
                // 후방이 벽으로 막혀있다면 좌/우 대각선 후방으로 탐색
                FVector LeftFlee = FRotator(0.f, 45.f, 0.f).RotateVector(AdjustedFleeDir);
                FVector RightFlee = FRotator(0.f, -45.f, 0.f).RotateVector(AdjustedFleeDir);
                if (NavSys->ProjectPointToNavigation(MyLoc + (LeftFlee * 450.0f), ProjectedNavLoc, FVector(150.f, 150.f, 250.f)))
                {
                    bFoundNav = true;
                }
                else if (NavSys->ProjectPointToNavigation(MyLoc + (RightFlee * 450.0f), ProjectedNavLoc, FVector(150.f, 150.f, 250.f)))
                {
                    bFoundNav = true;
                }
            }
        }

        const FVector FinalFleeLoc = bFoundNav ? ProjectedNavLoc.Location : DesiredLoc;

        // 후퇴 속도를 전투 달리기 속도로 설정
        if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
        {
            MoveComp->MaxWalkSpeed = (CombatRunSpeed > 0.0f) ? CombatRunSpeed : 450.0f;
        }

        // 재귀 호출 방지를 위해 MoveToLocation 호출 전에 먼저 후퇴 상태 플래그 및 타이머 설정
        bIsFleeing = true;
        bPendingHybridFlee = false;
        FleeEndTime = Now + 1.4f; // 최대 1.4초 동안 후퇴에 집중하여 안전 거리 확보
        LastFleeTime = Now;
        EnterRangeTime = -1.f; // 후퇴 완료 후 조준 뜸들이기(Aim Delay) 자동 유도!

        bIsIssuingFleeMove = true;
        AIC->MoveToLocation(FinalFleeLoc, 35.0f, false);
        bIsIssuingFleeMove = false;
        return true;
    }

    return false;
}

bool AEnemyCharacter::IsFleeing()
{
    if (!bIsFleeing) return false;

    UWorld* World = GetWorld();
    if (!World)
    {
        bIsFleeing = false;
        return false;
    }

    const float Now = World->GetTimeSeconds();
    if (Now >= FleeEndTime)
    {
        bIsFleeing = false; // ★ 시간 만료 시 플래그 리셋!
        return false;
    }

    // 몽타주가 없는 이동 카이팅인 경우, 목표 위치에 도착(이동 완료)했으면 종료 허용
    if (!BackstepMontage)
    {
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            // 최소 0.45초는 무조건 이동 보장
            if ((Now - (FleeEndTime - 1.4f)) > 0.45f && AIC->GetMoveStatus() != EPathFollowingStatus::Moving)
            {
                bIsFleeing = false; // ★ 이동 완료 시 플래그 리셋!
                return false;
            }
        }
    }

    return true;
}

void AEnemyCharacter::FinishFlee()
{
    bIsFleeing = false;
    bIsIssuingFleeMove = false;
    FleeEndTime = 0.f;
    LastFleeTime = -100.f; // 피격 등으로 후퇴가 중단되었을 때 피격 종료 직후 다시 후퇴할 수 있도록 쿨다운 해제
}



