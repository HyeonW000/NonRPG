#include "Character/EnemyCharacter.h"
#include "Net/UnrealNetwork.h" // [Fix] Required for DOREPLIFETIME

#include "Ability/NonAbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h" // [Fix] Required for accessing UGameplayAbility class
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagsManager.h"
#include "GameplayTagContainer.h"

#include "Animation/AnimMontage.h"

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

AEnemyCharacter::AEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

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
    FadeParamName = TEXT("Fade");

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

void AEnemyCharacter::InitFromDataTableRow(const FEnemyDataTableRow& InRow)
{
    EnemyDisplayName = InRow.Name;

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

    // 0) 공격 애니메이션 몽타주 및 사정거리 주입
    if (InRow.AttackMontages.Num() > 0)
    {
        AttackMontages = InRow.AttackMontages;
    }
    if (InRow.DeathMontage)
    {
        DeathMontage = InRow.DeathMontage;
    }

    // 0-1) 피격 애니메이션 몽타주 주입 (일반 몬스터 전용 - 약피격, 강피격, 넉다운, 스턴)
    if (InRow.HitLightMontage)
    {
        HitMontages.Add(FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit.Light")), InRow.HitLightMontage);
    }
    if (InRow.HitHeavyMontage)
    {
        HitMontages.Add(FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit.Heavy")), InRow.HitHeavyMontage);
    }
    if (InRow.KnockdownMontage)
    {
        HitMontages.Add(FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit.Knockdown")), InRow.KnockdownMontage);
        HitMontages.Add(FGameplayTag::RequestGameplayTag(TEXT("State.KnockDown")), InRow.KnockdownMontage);
    }
    if (InRow.StunMontage)
    {
        HitMontages.Add(FGameplayTag::RequestGameplayTag(TEXT("State.Stunned")), InRow.StunMontage);
        HitMontages.Add(FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit.Stun")), InRow.StunMontage);
    }
    if (InRow.AttackRange > 0.f)
    {
        AttackRange = InRow.AttackRange;
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
    if (InRow.FirstAttackWindupMin >= 0.f)
    {
        FirstAttackWindupMin = InRow.FirstAttackWindupMin;
    }
    if (InRow.FirstAttackWindupMax >= 0.f)
    {
        FirstAttackWindupMax = InRow.FirstAttackWindupMax;
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

    // 1) 추가 어빌리티/스킬 부여 (일반 적 및 보스 공통)
    if (AbilitySystemComponent && HasAuthority())
    {
        for (TSubclassOf<UGameplayAbility> AbilityClass : InRow.GrantedSkills)
        {
            if (AbilityClass)
            {
                AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
            }
        }
    }

    // 2) 보스 전용 페이즈 데이터 에셋(BossPhaseData) 자동 초기화
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
        // 1) EnemyDataTable이 미지정된 경우 기본 DT_EnemyData 에셋 자동 로드
        if (!EnemyDataTable)
        {
            EnemyDataTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Non/Data/DT_EnemyData.DT_EnemyData"));
            if (!EnemyDataTable)
            {
                EnemyDataTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Non/Data/DT_EnemyData"));
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
                UE_LOG(LogTemp, Warning, TEXT("[Enemy DT 동기화] %s | DT 행 '%s' (이름: %s) 성공적으로 동기화 완료!"),
                    *GetName(), *EnemyRowName.ToString(), *EnemyDisplayName.ToString());
            }
        }
    }

    // 레벨에 직접 배치되었거나 스포너 없이 스폰된 적 캐릭터도 스탯/어빌리티가 즉시 초기화되도록 보장
    if (HasAuthority())
    {
        InitializeAttributes();
    }

    if (HPBarWidget)
    {
        HPBarWidget->InitWidget();
        UpdateHPBar();
        HPBarWidget->SetVisibility(false);
    }

    // [Legacy Removed] AnimSet assignments (GAS uses GA_Death/GA_Hit)

    if (bUseSpawnFadeIn)
    {
        InitSpawnFadeMIDs();
        PlaySpawnFadeIn(SpawnFadeDuration);
    }

    if (InteractCollision)
    {
        InteractCollision->OnComponentBeginOverlap.AddDynamic(this, &AEnemyCharacter::OnInteractOverlapBegin);
        InteractCollision->OnComponentEndOverlap.AddDynamic(this, &AEnemyCharacter::OnInteractOverlapEnd);
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

    TickSpawnFade();
    TickCombatRotation(DeltaSeconds);
}

// [Removed] GetLifetimeReplicatedProps (Reverted Replication) -> Restored for EnemyData
void AEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AEnemyCharacter, EnemyData);
    DOREPLIFETIME(AEnemyCharacter, EnemyDisplayName);
    DOREPLIFETIME(AEnemyCharacter, EnemyRowName);
    DOREPLIFETIME(AEnemyCharacter, bLootAvailable);
}

void AEnemyCharacter::OnRep_EnemyData()
{
    if (EnemyData)
    {
        InitFromDataAsset(EnemyData);
    }
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
                    // 사망 판정 시 Effect.Death 이벤트 발송하여 GA_Death/GA_Enemy_Death 100% 확실한 자동 발동 보장!
                    if (AbilitySystemComponent)
                    {
                        FGameplayTag DeathEventTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Death"), false);
                        if (DeathEventTag.IsValid())
                        {
                            FGameplayEventData Payload;
                            Payload.EventTag = DeathEventTag;
                            Payload.Target = this;
                            AbilitySystemComponent->HandleGameplayEvent(DeathEventTag, &Payload);
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
        DebuffTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Debuff"), false));
        DebuffTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"), false));
        DebuffTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.HitReacting"), false));
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

    // 경험치 지급: 파티 보너스 및 거리 제한 균등 분배
    if (HasAuthority() && ExpReward > 0.f)
    {
        DistributeExpOnDeath(LastDamageInstigator.Get());
    }
    
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
        Move->Velocity = FVector::ZeroVector;
        Move->SetAvoidanceEnabled(false);
        Move->bUseRVOAvoidance = false;
    }
    
    // AI 정지 및 완전히 조종(빙의) 해제 (영구 좀비화 방지)
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (UBrainComponent* Brain = AIC->GetBrainComponent())
        {
            Brain->StopLogic("Death");
        }
        AIC->StopMovement();
        
        // 뇌(컨트롤러)와 몸통(캐릭터)의 연결을 아예 끊어버립니다.
        AIC->UnPossess();
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
    // 0) RVO 회피 시스템 즉시 해제 (보스나 다른 몬스터가 시체를 회피 대상으로 보지 않고 뚫고 지나가도록 함)
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->SetAvoidanceEnabled(false);
        Move->bUseRVOAvoidance = false;
    }

    // 1) 캡슐 콜리전: 크기는 절대 건드리지 않고, Pawn과 Camera만 통과(Ignore)!
    // 바닥 충돌(WorldStatic/Dynamic)은 그대로 Block 유지하여 땅 꺼짐 0% 방지!
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
        Capsule->CanCharacterStepUpOn = ECB_No;
    }

    // 2) 스켈레탈 메쉬(피직스 에셋 본 콜리전)도 Pawn과 Camera 통과!
    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        Skel->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        Skel->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        Skel->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
        Skel->CanCharacterStepUpOn = ECB_No;
    }

    // 3) 죽은 몬스터/보스에 부착된 모든 컴포넌트(히트박스, 보석 등)의 폰/카메라 충돌 제거!
    TArray<UPrimitiveComponent*> PrimComps;
    GetComponents<UPrimitiveComponent>(PrimComps);
    for (UPrimitiveComponent* PrimComp : PrimComps)
    {
        if (PrimComp && PrimComp != InteractCollision)
        {
            PrimComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
            PrimComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
            PrimComp->CanCharacterStepUpOn = ECB_No;
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
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->SetAvoidanceEnabled(false);
        Move->bUseRVOAvoidance = false;
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
        Capsule->CanCharacterStepUpOn = ECB_No;
    }

    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        // 1) 마지막 포즈에서 애니/틱 정지 (서버 및 모든 클라이언트 화면에서 즉시 고정)
        Skel->bPauseAnims = true;
        Skel->SetComponentTickEnabled(false);
        Skel->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        Skel->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        Skel->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
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
    FGameplayTag DefaultTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit.Light"), false);
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
    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        Skel->SetRenderCustomDepth(bEnable);
        Skel->SetCustomDepthStencilValue(bEnable ? 1 : 0);
    }

    TArray<UMeshComponent*> MeshComps;
    GetComponents<UMeshComponent>(MeshComps);
    for (UMeshComponent* MeshComp : MeshComps)
    {
        if (MeshComp)
        {
            MeshComp->SetRenderCustomDepth(bEnable);
            MeshComp->SetCustomDepthStencilValue(bEnable ? 1 : 0); // 1번 채널 (프로젝트 전역 아웃라인 채널)
        }
    }
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
    if (IsDead()) return;

    // ── [New] 동일 공격자 0.25초 이내 중복 타격 방지 (멀티플레이 중복 판정 2중 방어) ──
    if (DamageInstigator)
    {
        const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
        if (float* LastHitTime = RecentAttackerHitTimes.Find(DamageInstigator))
        {
            if ((CurrentTime - *LastHitTime) < 0.25f)
            {
                return; // 0.25초 이내 동일 공격자의 연속 타격 무시
            }
        }
        RecentAttackerHitTimes.Add(DamageInstigator, CurrentTime);
    }

    // ── 무적 / 회피 태그 체크 시 데미지 무시 및 "무적" / "회피" 텍스트 팝업 ──
    static const FGameplayTag Tag_Invincible = FGameplayTag::RequestGameplayTag(TEXT("State.Invincible"), false);
    static const FGameplayTag Tag_IFrame = FGameplayTag::RequestGameplayTag(TEXT("State.IFrame"), false);
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
                const FGameplayTag Tag_Damage = FGameplayTag::RequestGameplayTag(TEXT("Data.Damage"), false);
                Spec.Data->SetSetByCallerMagnitude(Tag_Damage, -Amount);

                // [New] Critical 정보 전달 (AttributeSet에서 확인용)
                if (bIsCritical)
                {
                    Spec.Data->AddDynamicAssetTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Damage.Critical"), false));
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
        if (!HitEventTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit"))))
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

    const float Cur = AttributeSet ? AttributeSet->GetHP() : 0.f;
    const float Max = AttributeSet ? AttributeSet->GetMaxHP() : 1.f;
    const bool bDamaged = (Cur < Max);

    // [Fix] HP바 표시 조건 완벽 복원:
    // 1) "전투 중에만 표시(bShowHPBarOnlyInCombat)" 옵션이 꺼져 있으면 상시 표시
    // 2) 켜져 있다면: 전투 중(bInCombat)이거나, 어그로 상태(bAggro)이거나, 체력이 깎였거나(bDamaged), 플레이어가 적중했을 때(bClientHPBarVisible) 표시!
    bool bShouldShow = false;
    if (!bShowHPBarOnlyInCombat)
    {
        bShouldShow = true;
    }
    else
    {
        bShouldShow = bInCombat || bAggro || bDamaged || bClientHPBarVisible;
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

void AEnemyCharacter::SetAggro(bool bNewAggro)
{
    if (bAggro == bNewAggro)
    {
        return;
    }

    bAggro = bNewAggro;

    if (!bAggro)
    {
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

    bIsAlerting = true;

    // 1) 즉시 이동만 정지 (몸을 휙 돌리지 않고, 가던 방향/바라보던 방향 그대로 멈춰서 경계)
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        AIC->StopMovement();
    }
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
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
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[경계 디버그] %s | bShowAlertMark가 false여서 AlertWidget을 띄우지 않습니다."), *GetName());
    }

    // 5) 블루프린트 이벤트 방송 (추가 사운드나 특수 연출 필요시 BP에서도 수신 가능)
    OnEnemyAlerted.Broadcast(InTarget);

    if (GEngine)
    {
        const FString Msg = FString::Printf(TEXT("[AI 경계] %s | 선공 감지! 경계 태세 (대기 텀: %.2f초)"), *GetName(), AlertDelay);
        GEngine->AddOnScreenDebugMessage((uint64)this + 999, 1.5f, FColor::Yellow, Msg);
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
    }

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

    if (GEngine)
    {
        const FString Msg = FString::Printf(TEXT("[경계 디버그] %s | AlertWidget 표시 성공! (클래스: %s, 지속시간: %.2f초)"), *GetName(), *WClass->GetName(), ShowDuration);
        GEngine->AddOnScreenDebugMessage((uint64)this + 777, 2.0f, FColor::Cyan, Msg);
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
    }

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
        Move->MaxWalkSpeed = (CombatRunSpeed > 0.0f) ? CombatRunSpeed : 400.0f;
    }

    if (GEngine)
    {
        const FString Msg = FString::Printf(TEXT("[AI 경계] %s | [3단계: 텀 종료] 플레이어에게 본격 접근 시작!"), *GetName());
        GEngine->AddOnScreenDebugMessage((uint64)this + 999, 1.5f, FColor::Orange, Msg);
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
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
    if (!TargetActor && GetWorld())
    {
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            TargetActor = PC->GetPawn();
        }
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
        const FGameplayTag MeleeTag = FGameplayTag::RequestGameplayTag(TEXT("Enemy.Skill.Melee"), false);
        if (MeleeTag.IsValid())
        {
            ActivateTags.AddTag(MeleeTag);
        }
        const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack"), false);
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

    // 플레이어만 맞추기
    if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(Other))
    {
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
                Player->ApplyDamageAt(FinalDamage, this, HitLoc);
                UpdateCombatActionTime(); // 공격 성공 시 전투 무반응 타이머 갱신
            }
        }
    }
}

// ── Fade Functions ──
void AEnemyCharacter::InitSpawnFadeMIDs()
{
    SpawnFadeMIDs.Reset();
    if (!GetMesh()) return;

    const int32 NumMats = GetMesh()->GetNumMaterials();
    for (int32 i = 0; i < NumMats; ++i)
    {
        UMaterialInterface* Mat = GetMesh()->GetMaterial(i);
        if (Mat)
        {
            UMaterialInstanceDynamic* MID = GetMesh()->CreateDynamicMaterialInstance(i, Mat);
            if (MID)
            {
                SpawnFadeMIDs.Add(MID);
            }
        }
    }
}

void AEnemyCharacter::PlaySpawnFadeIn(float Duration)
{
    bIsFadingOut = false; // FadeIn 모드
    if (Duration < 0.f) Duration = SpawnFadeDuration;
    if (Duration <= 0.01f)
    {
        // 즉시 완료
        for (auto& MID : SpawnFadeMIDs)
        {
            if (MID) MID->SetScalarParameterValue(FadeParamName, 1.f); // 1=보임
        }
        return;
    }

    SpawnFadeLen = Duration;
    SpawnFadeStartTime = GetWorld()->GetTimeSeconds();
    bSpawnFadeActive = true;

    // AI/이동 정지
    if (bPauseAIWhileFading)
    {
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            if (auto* Brain = AIC->GetBrainComponent()) Brain->StopLogic("SpawnFade");
            AIC->StopMovement();
        }
        if (auto* Move = GetCharacterMovement())
        {
            SavedMaxWalkSpeed = Move->MaxWalkSpeed;
            Move->MaxWalkSpeed = 0.f;  // 혹은 DisableMovement
        }
    }

    // 초기값 세팅 (0.0 = 투명에서 시작한다고 가정)
    for (auto& MID : SpawnFadeMIDs)
    {
        if (MID) MID->SetScalarParameterValue(FadeParamName, 0.f);
    }
}

void AEnemyCharacter::TickSpawnFade()
{
    if (!bSpawnFadeActive) return;

    float Now = GetWorld()->GetTimeSeconds();
    float Elapsed = Now - SpawnFadeStartTime;
    float Alpha = FMath::Clamp(Elapsed / SpawnFadeLen, 0.f, 1.f);

    // FadeIn(0->1), FadeOut(1->0)
    float Val = bIsFadingOut ? (1.f - Alpha) : Alpha;

    for (auto& MID : SpawnFadeMIDs)
    {
        if (MID) MID->SetScalarParameterValue(FadeParamName, Val);
    }

    if (Alpha >= 1.f)
    {
        bSpawnFadeActive = false;
        
        if (bIsFadingOut)
        {
            Destroy(); // 페이드 아웃 끝나면 삭제
        }
        else
        {
            OnSpawnFadeFinished(); // 페이드 인 끝나면 AI 시작
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

    // 2) 공격 몽타주나 피격 등 어떤 몽타주라도 재생 중이면 회전 100% 완전 잠금(Lock)!
    if (GetCurrentMontage() != nullptr)
    {
        return;
    }

    // 3) GAS 태그(공격 중, 피격 중) 검사하여 회전 완전 차단
    if (AbilitySystemComponent)
    {
        static const FGameplayTag StateAttackingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
        static const FGameplayTag StateHitReactTag = FGameplayTag::RequestGameplayTag(TEXT("State.HitReact"), false);
        if (StateAttackingTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(StateAttackingTag))
        {
            return;
        }
        if (StateHitReactTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(StateHitReactTag))
        {
            return;
        }
    }

    // 4) 이미 이동 중이면 CharacterMovement의 bOrientRotationToMovement가 알아서 회전하므로 패스
    if (!GetVelocity().IsNearlyZero(10.f))
    {
        return;
    }

    // 타겟 액터 찾기
    AActor* TargetActor = nullptr;
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
        {
            TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
        }
    }
    if (!TargetActor && GetWorld())
    {
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            TargetActor = PC->GetPawn();
        }
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

void AEnemyCharacter::PlaySpawnFadeOut_Implementation(float Duration)
{
    if (Duration <= 0.f) Duration = 1.0f;
    
    // 만약 클라이언트에서 MID가 아직 없다면 생성 시도 (안전장치)
    if (SpawnFadeMIDs.Num() == 0)
    {
        InitSpawnFadeMIDs();
    }

    SpawnFadeLen = Duration;
    SpawnFadeStartTime = GetWorld()->GetTimeSeconds();
    bSpawnFadeActive = true;
    bIsFadingOut = true;

    // 아웃라인도 끄기
    SetInteractionOutline(false);
}

void AEnemyCharacter::OnSpawnFadeFinished()
{
    // 이동/AI 재개
    if (bPauseAIWhileFading)
    {
        if (bAutoResumeAfterFade)
        {
            if (AAIController* AIC = Cast<AAIController>(GetController()))
            {
                if (auto* Brain = AIC->GetBrainComponent()) Brain->RestartLogic();
            }
            if (auto* Move = GetCharacterMovement())
            {
                if (SavedMaxWalkSpeed > 0.f) Move->MaxWalkSpeed = SavedMaxWalkSpeed;
                else Move->MaxWalkSpeed = 350.f; // fallback
            }
        }
    }
}

void AEnemyCharacter::DistributeExpOnDeath(ANonCharacterBase* KillerChar)
{
    if (!HasAuthority() || ExpReward <= 0.f || !KillerChar) return;

    ANonPlayerController* KillerPC = Cast<ANonPlayerController>(KillerChar->GetController());
    UPartyComponent* PartyComp = KillerPC ? KillerPC->GetPartyComponent() : nullptr;

    // 솔로 플레이어이거나 파티에 속해 있지 않은 경우
    if (!PartyComp || !PartyComp->IsInParty())
    {
        KillerChar->GainExp(ExpReward);
        if (KillerPC)
        {
            KillerPC->Client_AddSystemMessage(FString::Printf(TEXT("경험치 %.0f 을 획득했습니다."), ExpReward));
        }
        return;
    }

    // 파티 플레이인 경우: 거리 제한(35m) 이내의 유효 파티원 검색
    const FPartyData& PartyData = PartyComp->GetPartyData();
    TArray<ANonCharacterBase*> EligibleMembers;
    TArray<ANonPlayerController*> EligiblePCs;

    const FVector MonsterLoc = GetActorLocation();

    for (const FPartyMemberInfo& Member : PartyData.Members)
    {
        ANonCharacterBase* MemberChar = Member.Character.Get();
        ANonPlayerController* MemberPC = Member.PlayerController.Get();

        if (MemberChar && !MemberChar->IsDead())
        {
            const float Dist = FVector::Dist(MonsterLoc, MemberChar->GetActorLocation());
            if (Dist <= PartyExpShareRadius)
            {
                EligibleMembers.Add(MemberChar);
                if (MemberPC)
                {
                    EligiblePCs.Add(MemberPC);
                }
            }
        }
    }

    const int32 Count = EligibleMembers.Num();
    if (Count <= 0)
    {
        // 거리 내 아무도 없으면 마지막 타격자에게 전액 지급
        KillerChar->GainExp(ExpReward);
        return;
    }

    if (Count == 1)
    {
        // 거리 내 1명만 있으면 보너스 없이 100% 지급
        EligibleMembers[0]->GainExp(ExpReward);
        if (EligiblePCs.Num() > 0 && EligiblePCs[0])
        {
            EligiblePCs[0]->Client_AddSystemMessage(FString::Printf(TEXT("경험치 %.0f 을 획득했습니다."), ExpReward));
        }
        return;
    }

    // 2인 이상: 인원수당 보너스 추가 균등 분배!
    // 2인: 110%, 3인: 120%, 4인: 130%
    const float TotalExpWithBonus = ExpReward * (1.0f + (Count - 1) * PartyExpBonusPerMember);
    const float ExpPerMember = TotalExpWithBonus / static_cast<float>(Count);

    for (int32 i = 0; i < Count; ++i)
    {
        EligibleMembers[i]->GainExp(ExpPerMember);
        if (i < EligiblePCs.Num() && EligiblePCs[i])
        {
            EligiblePCs[i]->Client_AddSystemMessage(
                FString::Printf(TEXT("경험치 %.0f 을 획득했습니다."), ExpPerMember));
        }
    }
}

// ── Interaction (개인 루팅 / Instanced Looting) ──
void AEnemyCharacter::EnableCorpseInteraction()
{
    if (InteractCollision)
    {
        InteractCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        InteractCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
        InteractCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        InteractCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    }
    if (GetCapsuleComponent())
    {
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    }
    if (GetMesh())
    {
        GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    }
    bLootAvailable = true;
    bLooted = false;
    LootedCharacters.Empty();

    // 시체 수명 타이머 (기본 120초 유지)
    StartCorpseTimer(CorpseLifeTime);
}

void AEnemyCharacter::Interact_Implementation(ANonCharacterBase* Interactor)
{
    // 죽은 몬스터가 아니거나 인터랙터가 없으면 무시
    if (!IsDead() || !Interactor) return;

    // ── [개인 루팅 보호] 이미 이 플레이어가 획득했는지 확인 ──
    if (LootedCharacters.Contains(Interactor))
    {
        if (ANonPlayerController* PC = Cast<ANonPlayerController>(Interactor->GetController()))
        {
            PC->Client_OnCorpseLooted(this);
            PC->Client_AddSystemMessage(TEXT("[시스템] 이미 전리품을 획득한 시체입니다."));
        }
        return;
    }

    // 루팅 기록 추가 (개인 루팅 완료)
    LootedCharacters.Add(Interactor);

    ANonPlayerController* PC = Cast<ANonPlayerController>(Interactor->GetController());
    if (PC)
    {
        // 루팅 완료 즉시 클라이언트 화면에서 외곽선 끄기 및 프롬프트 닫기
        PC->Client_OnCorpseLooted(this);
    }

    // ── [개인별 독립 드롭 아이템 추첨 및 지급] ──
    if (!EnemyData)
    {
        if (PC)
        {
            PC->Client_AddSystemMessage(TEXT("[시스템] 이 몬스터에 EnemyData(데이터 에셋)가 설정되지 않아 전리품이 없습니다."));
        }
        return;
    }

    UInventoryComponent* Inv = Interactor->GetInventoryComponent();
    if (!Inv)
    {
        Inv = Interactor->FindComponentByClass<UInventoryComponent>();
    }

    if (!Inv)
    {
        if (PC)
        {
            PC->Client_AddSystemMessage(TEXT("[시스템] 플레이어 인벤토리 컴포넌트를 찾을 수 없습니다."));
        }
        return;
    }

    int32 DroppedCount = 0;
    for (const FEnemyDropItem& Drop : EnemyData->DropList)
    {
        const float Roll = FMath::FRand();
        if (Roll <= Drop.DropChance)
        {
            const int32 Quantity = FMath::RandRange(Drop.MinCount, Drop.MaxCount);
            if (Quantity > 0)
            {
                int32 OutSlotIndex = -1;
                if (Inv->AddItem(Drop.ItemId, Quantity, OutSlotIndex))
                {
                    DroppedCount++;
                    if (PC)
                    {
                        PC->Client_AddSystemMessage(FString::Printf(TEXT("[전리품] %s x%d 획득!"), *Drop.ItemId.ToString(), Quantity));
                    }
                }
                else
                {
                    if (PC)
                    {
                        PC->Client_AddSystemMessage(FString::Printf(TEXT("[시스템] 아이템 '%s' 지급 실패 (아이템 데이터 테이블에 해당 ID가 있는지 확인 필요)"), *Drop.ItemId.ToString()));
                    }
                }
            }
        }
    }

    if (DroppedCount == 0 && PC)
    {
        PC->Client_AddSystemMessage(TEXT("[전리품] 획득할 수 있는 아이템이 없습니다."));
    }
}

FText AEnemyCharacter::GetInteractLabel_Implementation()
{
    if (!IsDead() || !bLootAvailable) return FText::GetEmpty();
    return FText::FromString(TEXT("획득하기"));
}

void AEnemyCharacter::SetInteractHighlight_Implementation(bool bEnable)
{
    if (!IsDead())
    {
        SetInteractionOutline(false);
        return;
    }
    SetInteractionOutline(bEnable);
}

void AEnemyCharacter::OnInteractOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!bLootAvailable || bLooted) return;
    
    // 플레이어가 근처에 오면 뭔가 표시할 수도 있음
}

void AEnemyCharacter::OnInteractOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // 범위 벗어남
}

void AEnemyCharacter::StartCorpseTimer(float LifeTime)
{
    GetWorldTimerManager().ClearTimer(CorpseRemoveTimerHandle);
    if (LifeTime <= 0.f)
    {
        OnCorpseExpired();
    }
    else
    {
        GetWorldTimerManager().SetTimer(CorpseRemoveTimerHandle, this, &AEnemyCharacter::OnCorpseExpired, LifeTime, false);
    }
}

void AEnemyCharacter::OnCorpseExpired()
{
    // 바로 삭제하지 않고 페이드 아웃 시작
    PlaySpawnFadeOut(CorpseFadeOutDuration);
}

// ── 경직(이동 정지) ── [Legacy logic removed] - GA_HitReaction handles this if needed (Tags/Tasks)
// void AEnemyCharacter::StartHitMovePause(float OverrideDuration, bool bForceStop) { ... }
// void AEnemyCharacter::EndHitMovePause() { ... }

void AEnemyCharacter::BlockAttackFor(float Seconds)
{
    if (Seconds <= 0.f) return;
    const float Now = GetWorld()->GetTimeSeconds();
    NextAttackAllowedTime = Now + Seconds;
}

bool AEnemyCharacter::IsAttackAllowed() const
{
    const float Now = GetWorld()->GetTimeSeconds();
    return (Now >= NextAttackAllowedTime);
}


// (Windup) 관련 로직은 BT의 Cooldown/Wait 사용하므로 제거됨
void AEnemyCharacter::MarkEnteredAttackRange()
{
    if (EnterRangeTime < 0.f)
    {
        EnterRangeTime = GetWorld()->GetTimeSeconds();
        // 진입 할 때마다 랜덤 딜레이 결정
        CurrentWindupTime = FMath::RandRange(FirstAttackWindupMin, FirstAttackWindupMax);
        

    }
}

bool AEnemyCharacter::IsFirstAttackWindupDone() const
{
    if (EnterRangeTime < 0.f) return true; // 에러 방지

    const float Now = GetWorld()->GetTimeSeconds();
    return (Now - EnterRangeTime) >= CurrentWindupTime;
}

void AEnemyCharacter::BindASCDelegates()
{
    if (AbilitySystemComponent)
    {
        FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"), false);
        if (StunTag.IsValid())
        {
            AbilitySystemComponent->RegisterGameplayTagEvent(StunTag, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
            AbilitySystemComponent->RegisterGameplayTagEvent(StunTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AEnemyCharacter::DebugStunTagChanged);
        }
    }
}

void AEnemyCharacter::DebugStunTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    FString CasterRole = HasAuthority() ? TEXT("Server") : TEXT("Client");
    UE_LOG(LogTemp, Warning, TEXT("[StunDebug][%s] Enemy Actor: %s | Stun Tag count changed to: %d"), *CasterRole, *GetName(), NewCount);
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


