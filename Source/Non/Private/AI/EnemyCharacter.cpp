#include "AI/EnemyCharacter.h"

#include "Ability/NonAbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"
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

#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

#include "AIController.h"
#include "AI/NonAIController.h"
#include "Animation/EnemyAnimSet.h"
#include "Effects/DamageNumberActor.h"
#include "BrainComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Data/EnemyDataAsset.h"
#include "Combat/NonDamageHelpers.h" 

AEnemyCharacter::AEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    AbilitySystemComponent = CreateDefaultSubobject<UNonAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

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

    // 이동
    bUseControllerRotationYaw = false;
    if (auto* Move = GetCharacterMovement())
    {
        Move->bOrientRotationToMovement = true;
        Move->RotationRate = FRotator(0, 540, 0);
        Move->MaxWalkSpeed = 350.f;
    }

    // 히트박스 (기본 Off, 애님에서 On/Off)
    AttackHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackHitbox"));
    AttackHitbox->SetupAttachment(GetMesh(), TEXT("hand_r")); // 필요한 소켓명으로 교체 가능
    AttackHitbox->InitBoxExtent(FVector(15.f, 40.f, 40.f));
    AttackHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    AttackHitbox->SetCollisionObjectType(ECC_WorldDynamic);
    AttackHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
    AttackHitbox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    AttackHitbox->SetGenerateOverlapEvents(false);

    AttackHitbox->OnComponentBeginOverlap.AddDynamic(this, &AEnemyCharacter::OnAttackHitBegin);

    //Fade
    SpawnFadeDuration = 0.6f;
    bUseSpawnFadeIn = true;
    FadeParamName = TEXT("Fade");

    InteractCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractCollision"));
    InteractCollision->SetupAttachment(GetRootComponent());
    InteractCollision->InitSphereRadius(CorpseInteractRadius);
    InteractCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InteractCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
}

//  EnemyDataAsset에서 기본값 세팅
void AEnemyCharacter::InitFromDataAsset(const UEnemyDataAsset* InData)
{
    if (!InData) return;

    EnemyData = InData;
    BaseAttack = InData->Attack;
    BaseDefense = InData->Defense;
    ExpReward = InData->ExpReward;

    // MaxHP는 지금 쓰고 있는 StatInit / AttributeSet 구조에 맞춰
    // 나중에 여기서 연동하거나, 별도 초기화 로직에서 InData->MaxHP 사용하면 됨.
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
    }
    BindAttributeDelegates();

    if (HPBarWidget)
    {
        HPBarWidget->InitWidget();
        UpdateHPBar();
        HPBarWidget->SetVisibility(false);
    }

    if (AnimSet)
    {
    if (AnimSet)
    {
        // Death 설정은 DataAsset 기준으로 항상 덮어쓰기
        bUseDeathMontage = AnimSet->bUseDeathMontage;
        DeathMontage = AnimSet->DeathMontage;
    }
    }

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
    UpdateHPBarVisibility();
}

void AEnemyCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    TickSpawnFade();
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

#if WITH_EDITOR
    if (AttributeSet)
    {
        const float CurHP = AttributeSet->GetHP();
        const float MaxHP = AttributeSet->GetMaxHP();
        const float Atk = AttributeSet->GetAttackPower();
        const float Def = AttributeSet->GetDefense();

        UE_LOG(LogTemp, Warning,
            TEXT("[EnemyInit] HP=%f / MaxHP=%f / Atk=%f / Def=%f"),
            CurHP, MaxHP, Atk, Def);
    }
#endif
}

void AEnemyCharacter::BindAttributeDelegates()
{
    if (!AbilitySystemComponent || !AttributeSet) return;

    AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHPAttribute())
        .AddLambda([this](const FOnAttributeChangeData& Data)
            {
                UpdateHPBar();
                UpdateHPBarVisibility();

                if (Data.OldValue > 0.f && Data.NewValue <= 0.f)
                {
                    HandleDeath();
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
}

bool AEnemyCharacter::IsDead() const
{
    if (!AttributeSet) return false;
    if (AttributeSet->GetHP() <= 0.f) return true;
    return HasDeadTag();
}

void AEnemyCharacter::HandleDeath()
{
    if (bDied) return;
    bDied = true;

    // 죽자마자 바로 HP바 끄기
    if (HPBarWidget)
    {
        HPBarWidget->SetVisibility(false);
    }

    OnEnemyDied.Broadcast(this);

    // 경험치 지급: 마지막으로 나를 공격한 플레이어에게 ExpReward 만큼
    if (HasAuthority() && ExpReward > 0.f)
    {
        if (LastDamageInstigator.IsValid())
        {
            LastDamageInstigator->GainExp(ExpReward);
        }
    }
    // 여기부터: 시체 상호작용 가능 상태로 전환

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->DisableMovement();
        Move->StopMovementImmediately();
    }
    
    // AI 정지 (BT 실행 중단)
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (UBrainComponent* Brain = AIC->GetBrainComponent())
        {
            Brain->StopLogic("Death");
        }
        AIC->StopMovement();
    }

    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AddDeadTag();

    if (bUseDeathMontage && DeathMontage && GetMesh())
    {
        const float Len = PlayAnimMontage(DeathMontage, 1.0f);
        if (HPBarWidget) HPBarWidget->SetVisibility(false);

        if (Len > 0.f)
        {
            // 몽타주 BlendOut 시간
            const float BlendOutTime = DeathMontage->BlendOut.GetBlendTime();
            // BlendOut 시작 직전 시점에 얼리기
            const float FreezeDelay = FMath::Max(0.f, Len - BlendOutTime);

            GetWorldTimerManager().SetTimer(
                DeathPoseFreezeTimerHandle,
                this,
                &AEnemyCharacter::FreezeDeathPose,
                FreezeDelay,
                false
            );
        }

        return;
    }

    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        Skel->SetCollisionProfileName(FName("Ragdoll"));
        Skel->SetAllBodiesSimulatePhysics(true);
        Skel->SetSimulatePhysics(true);
        Skel->WakeAllRigidBodies();
    }    
    EnableCorpseInteraction();
    // Destroy는 여기서 바로 하지 않는다 (시체 남기기)
}

void AEnemyCharacter::FreezeDeathPose()
{
    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        // 1) 마지막 포즈에서 애니/틱 정지
        Skel->bPauseAnims = true;
        Skel->SetComponentTickEnabled(false);

        // 2) 시체 중심 위치 계산 (pelvis 기준, 없으면 Bounds 중심)
        if (InteractCollision)
        {
            // 스켈레톤에서 실제 골반 뼈 이름 확인해서 바꿔줘
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

void AEnemyCharacter::SetInteractionOutline(bool bEnable)
{
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->SetRenderCustomDepth(bEnable);
        MeshComp->SetCustomDepthStencilValue(bEnable ? 1 : 0); // 1번 채널 사용한다고 가정
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
    if (Amount <= 0.f || !AbilitySystemComponent || !AttributeSet) return;
    if (IsDead()) return;

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

    // ── 실제 HP 감소 (GAS 우선)
    if (GE_Damage)
    {
        FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
        Ctx.AddInstigator(DamageInstigator, Cast<APawn>(DamageInstigator) ? Cast<APawn>(DamageInstigator)->GetController() : nullptr);

        FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(GE_Damage, 1.f, Ctx);
        if (Spec.IsValid())
        {
            const FGameplayTag Tag_Damage = FGameplayTag::RequestGameplayTag(TEXT("Data.Damage"));
            Spec.Data->SetSetByCallerMagnitude(Tag_Damage, -Amount);

            // [New] Critical 정보 전달 (AttributeSet에서 확인용)
            if (bIsCritical)
            {
                Spec.Data->AddDynamicAssetTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Damage.Critical"), false));
            }

            AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        }
    }
    else
    {
        ApplyHealthDelta_Direct(-Amount);
    }

    // 데미지 숫자 (이제 AttributeSet에서 최종 데미지로 띄움)
    // Multicast_SpawnDamageNumber(Amount, WorldLocation, bIsCritical);

    // 피격 리액션
    OnGotHit(Amount, DamageInstigator, WorldLocation, ReactionTag);

    // Reactive 어그로: "맞아서 어그로" 플래그 + 타임스탬프 기록
    if (AggroStyle == EAggroStyle::Reactive)
    {
        MarkAggroByHit(DamageInstigator);   // bAggroByHit = true, LastAggroByHitTime 업데이트
        SetAggro(true);
    }

    // 전투 상태 진입(HP바 표시 등)
    EnterCombat();
}

void AEnemyCharacter::MarkAggroByHit(AActor* InstigatorActor)
{
    // 누가 때렸는지까지 쓰고 싶으면 여기서 저장/검증 가능
    bAggroByHit = true;
    LastAggroByHitTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void AEnemyCharacter::Multicast_SpawnDamageNumber_Implementation(float Amount, FVector WorldLocation, bool bIsCritical)
{
    if (!GetWorld()) return;

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

void AEnemyCharacter::EnterCombat()
{
    bInCombat = true;
    UpdateHPBarVisibility();

    GetWorldTimerManager().ClearTimer(CombatTimeoutTimer);
    GetWorldTimerManager().SetTimer(CombatTimeoutTimer, this, &AEnemyCharacter::LeaveCombat, CombatTimeout, false);
}

void AEnemyCharacter::LeaveCombat()
{
    bInCombat = false;
    UpdateHPBarVisibility();
}

void AEnemyCharacter::UpdateHPBarVisibility()
{
    if (!HPBarWidget) return;

    // 죽었으면 그냥 바로 끔
    if (IsDead())
    {
        HPBarWidget->SetVisibility(false);
        return;
    }

    const float Cur = AttributeSet ? AttributeSet->GetHP() : 0.f;
    const float Max = AttributeSet ? AttributeSet->GetMaxHP() : 1.f;

    bool bShouldShow = true;
    if (bShowHPBarOnlyInCombat)
    {
        // 전투 중이거나, 어그로 상태면 HP바 표시
        bShouldShow = (bInCombat || bAggro);
    }
    else
    {
        // 평소엔 맞아서 피가 깎였거나, 어그로 중이면 표시
        bShouldShow = ((Cur < Max - KINDA_SMALL_NUMBER) || bAggro);
    }

    HPBarWidget->SetVisibility(bShouldShow);
}

void AEnemyCharacter::SetAggro(bool bNewAggro)
{
    if (bAggro == bNewAggro)
    {
        return;
    }

    bAggro = bNewAggro;

    // 어그로 상태가 바뀌면 HP바도 즉시 갱신
    UpdateHPBarVisibility();
}

void AEnemyCharacter::TryStartAttack()
{
    EnterCombat();

    if (!IsAttackAllowed()) 
    {
        return;
    }

    if (AbilitySystemComponent)
    {
        const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack"), false);
        if (AttackTag.IsValid())
        {
            FGameplayTagContainer ActivateTags; ActivateTags.AddTag(AttackTag);
            if (AbilitySystemComponent->TryActivateAbilitiesByTag(ActivateTags))
            {
                return;
            }
        }
    }

    PlayAttackMontage();
}

// [Updated] for Hit Reaction Tag (Knockdown)
void AEnemyCharacter::OnGotHit(float Damage, AActor* InstigatorActor, const FVector& ImpactPoint, FGameplayTag ReactionTag)
{
    ApplyHitStopSelf(0.2f, 0.06f);

    if (!bCanHitReact || IsDead() || Damage <= 0.f)
    {
        return;
    }

    // [New] Knockdown Check
    static const FGameplayTag KnockdownTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Reaction.Knockdown"));
    if (ReactionTag.MatchesTag(KnockdownTag))
    {
        // 1) Play Knockdown Montage
        float MontageDuration = 0.f;
        if (AnimSet && AnimSet->KnockdownMontage && GetMesh() && GetMesh()->GetAnimInstance())
        {
             const float Duration = PlayAnimMontage(AnimSet->KnockdownMontage);
             MontageDuration = Duration; // Store duration for pause
             
             if (Duration <= 0.f)
             {
                 // Log removed
             }
        }


        // 2) Force Launch (Remove as requested)
        // const FVector Dir = ComputeKnockbackDir(InstigatorActor, ImpactPoint);
        // FVector Impulse = Dir * LaunchStrength * 1.5f; 
        // Impulse.Z = (LaunchUpward > 0.f) ? LaunchUpward : 400.f; 
        // LaunchCharacter(Impulse, true, true);

        // CD
        bCanHitReact = false;
        GetWorldTimerManager().SetTimer(HitReactCDTimer, [this]() { bCanHitReact = true; }, HitReactCooldown, false);
        
        // 넉다운 시간만큼 이동 정지 (Force Stop)
        StartHitMovePause(MontageDuration, true);
        
        // 넉다운 중인 동안 (+ 일어난 직후 딜레이) 공격 금지
        BlockAttackFor(MontageDuration + AttackDelayAfterHit);
        return; 
    }

    bCanHitReact = false;
    GetWorldTimerManager().SetTimer(HitReactCDTimer, [this]() { bCanHitReact = true; }, HitReactCooldown, false);

    PlayHitReact(ComputeHitQuadrant(ImpactPoint));

    // 피격 넉백(Launch)
    if (KnockbackMode == EKnockbackMode::Launch)
    {
        const FVector Dir = ComputeKnockbackDir(InstigatorActor, ImpactPoint);

        FVector Impulse = Dir * LaunchStrength;

        // Z 성분은 필요할 때만 추가 + 오버라이드
        const bool bUseZ = FMath::Abs(LaunchUpward) > KINDA_SMALL_NUMBER;
        if (bUseZ)
        {
            Impulse.Z += LaunchUpward;
            LaunchCharacter(Impulse, /*bXYOverride=*/true, /*bZOverride=*/true);
        }
        else
        {
            LaunchCharacter(Impulse, /*bXYOverride=*/true, /*bZOverride=*/false); // 기본: 붕 안뜸
        }
    }
    // 피격 후 잠깐 이동 멈춤
    StartHitMovePause();

    // 피격 후 잠깐 공격 금지
    BlockAttackFor(AttackDelayAfterHit);
}

EHitQuadrant AEnemyCharacter::ComputeHitQuadrant(const FVector& ImpactPoint) const
{
    const FVector Fwd = GetActorForwardVector();
    const FVector Right = GetActorRightVector();
    const FVector ToHit = (ImpactPoint - GetActorLocation()).GetSafeNormal2D();

    const float FwdDot = FVector::DotProduct(Fwd, ToHit);
    const float RightDot = FVector::DotProduct(Right, ToHit);

    if (FMath::Abs(FwdDot) >= FMath::Abs(RightDot))
    {
        return (FwdDot >= 0.f) ? EHitQuadrant::Front : EHitQuadrant::Back;
    }
    else
    {
        return (RightDot >= 0.f) ? EHitQuadrant::Right : EHitQuadrant::Left;
    }
}

void AEnemyCharacter::PlayHitReact(EHitQuadrant Quad)
{
    if (!AnimSet) return;
    
    UAnimMontage* M = nullptr;
    switch (Quad)
    {
    case EHitQuadrant::Front: M = AnimSet->HitReact_F; break;
    case EHitQuadrant::Back:  M = AnimSet->HitReact_B; break;
    case EHitQuadrant::Left:  M = AnimSet->HitReact_L; break;
    case EHitQuadrant::Right: M = AnimSet->HitReact_R; break;
    }
    if (!M) M = AnimSet->HitReact_F;
    if (!M || !GetMesh()) return;

    if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
    {
        Anim->Montage_Play(M, 1.0f);
    }
}

FVector AEnemyCharacter::ComputeKnockbackDir(AActor* InstigatorActor, const FVector& ImpactPoint) const
{
    // 1) 충돌 지점 기준: ImpactPoint -> 나 (수평)
    if (!ImpactPoint.IsNearlyZero())
    {
        FVector D = (GetActorLocation() - ImpactPoint);
        D.Z = 0.f;
        if (!D.IsNearlyZero()) return D.GetSafeNormal();
    }

    // 2) 가해자 기준: Instigator -> 나 (수평)
    if (InstigatorActor)
    {
        FVector D = (GetActorLocation() - InstigatorActor->GetActorLocation());
        D.Z = 0.f;
        if (!D.IsNearlyZero()) return D.GetSafeNormal();
    }

    // 3) 가해자 전방 반대 추정
    if (const APawn* P = Cast<APawn>(InstigatorActor))
    {
        FVector D = -P->GetActorForwardVector();
        D.Z = 0.f;
        if (!D.IsNearlyZero()) return D.GetSafeNormal();
    }

    // 4) 폴백: 내 전방 반대
    FVector D = -GetActorForwardVector();
    D.Z = 0.f;
    return D.GetSafeNormal();
}

void AEnemyCharacter::ApplyHitStopSelf(float Scale, float Duration)
{
    Scale = FMath::Clamp(Scale, 0.05f, 1.f);
    CustomTimeDilation = Scale;

    GetWorldTimerManager().ClearTimer(HitStopTimer);
    GetWorldTimerManager().SetTimer(HitStopTimer, [this]()
        {
            CustomTimeDilation = 1.f;
        }, Duration, false);
}

UAnimMontage* AEnemyCharacter::PickAttackMontage() const
{
    if (!AnimSet) return nullptr;
    const TArray<TObjectPtr<UAnimMontage>>& List = AnimSet->AttackMontages;
    if (List.Num() <= 0) return nullptr;

    const int32 Index = FMath::RandRange(0, List.Num() - 1);
    return List[Index];
}

void AEnemyCharacter::PlayAttackMontage()
{
    if (IsDead() || !GetMesh()) return;

    if (UAnimMontage* M = PickAttackMontage())
    {
        if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
        {
            Anim->Montage_Play(M, 1.0f);
        }
    }
}

void AEnemyCharacter::AttackHitbox_Enable()
{
    // 한 스윙 윈도우 동안 중복 타격 방지용
    HitOnce.Reset();

    if (AttackHitbox)
    {
        // 충돌 켜기 + 오버랩 이벤트 켜기
        AttackHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        AttackHitbox->SetGenerateOverlapEvents(true);

        // (선택) 델리게이트가 중복으로 붙는 게 걱정되면 아래 두 줄 사용
        // AttackHitbox->OnComponentBeginOverlap.RemoveDynamic(this, &AEnemyCharacter::OnAttackHitBegin);
        // AttackHitbox->OnComponentBeginOverlap.AddDynamic(this, &AEnemyCharacter::OnAttackHitBegin);
    }
}

void AEnemyCharacter::AttackHitbox_Disable()
{
    if (AttackHitbox)
    {
        AttackHitbox->SetGenerateOverlapEvents(false);
        AttackHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        // (선택) 위 Enable에서 델리게이트를 붙였다면 여기서 해제
        // AttackHitbox->OnComponentBeginOverlap.RemoveDynamic(this, &AEnemyCharacter::OnAttackHitBegin);
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
            const FVector From = AttackHitbox ? AttackHitbox->GetComponentLocation() : GetActorLocation();

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
        if (UWorld* W = GetWorld())
        {
            DrawDebugSphere(W, HitLoc, 10.f, 12, FColor::Yellow, false, 2.0f);
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

// ── Interaction ──
void AEnemyCharacter::EnableCorpseInteraction()
{
    if (InteractCollision)
    {
        InteractCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
    bLootAvailable = true;
    bLooted = false;

    // 시체 수명 타이머 (루팅 안해도 120초 뒤 삭제)
    StartCorpseTimer(CorpseLifeTime);
}

void AEnemyCharacter::Interact_Implementation(ANonCharacterBase* Interactor)
{
    if (!bLootAvailable || bLooted) return;

    if (Interactor)
    {
        // 예: 아이템 드랍
        // ... (Drop Logic) ...
        UE_LOG(LogTemp, Log, TEXT("[Enemy] Looted by %s!"), *Interactor->GetName());
        
        // 인벤토리에 아이템 추가 로직 등등 호출
        // Interactor->GetInventory()->Add...
    }

    bLooted = true;
    bLootAvailable = false;

    // 루팅 후 빠르게 삭제
    StartCorpseTimer(CorpseLifeTimeAfterLoot);

    // 아웃라인 끄기
    SetInteractionOutline(false);

    // [Fix] UI 즉시 갱신을 위해 콜리전 끄기 (Player EndOverlap 유도)
    if (InteractCollision)
    {
        InteractCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

FText AEnemyCharacter::GetInteractLabel_Implementation()
{
    if (!bLootAvailable) return FText::GetEmpty();
    if (bLooted) return FText::FromString(TEXT("Empty"));
    return FText::FromString(TEXT("Loot"));
}

void AEnemyCharacter::SetInteractHighlight_Implementation(bool bEnable)
{
    if (!bLootAvailable || bLooted)
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

// ── 경직(이동 정지) ──
void AEnemyCharacter::StartHitMovePause(float OverrideDuration, bool bForceStop)
{
    const float Duration = (OverrideDuration > 0.f) ? OverrideDuration : HitMovePauseDuration;
    if (Duration <= 0.f) return;

    // 이미 진행 중이면 타이머만 연장? 아니면 새로 시작? -> 새로 시작
    GetWorldTimerManager().ClearTimer(HitMovePauseTimer);

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        // 1) 속도 저하
        float Scale = bForceStop ? 0.f : HitMoveSpeedScale;
        Move->MaxWalkSpeed = 350.f * Scale; // 350은 기본값이라 쳤을 때
        if (Scale <= KINDA_SMALL_NUMBER)
        {
            Move->StopMovementImmediately();
        }
    }

    // 2) AI 일시 정지
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (auto* Brain = AIC->GetBrainComponent()) Brain->PauseLogic("HitPause");
    }

    GetWorldTimerManager().SetTimer(HitMovePauseTimer, this, &AEnemyCharacter::EndHitMovePause, Duration, false);
}

void AEnemyCharacter::EndHitMovePause()
{
    // 속도 복구
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        // 원래 속도는 DataAsset이나 멤버변수에 저장해두는 게 정석이지만, 일단 하드코딩 350 or BaseSpeed
        Move->MaxWalkSpeed = 350.f;  
    }
    
    // AI 재개
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        if (auto* Brain = AIC->GetBrainComponent()) Brain->ResumeLogic("HitPause");
    }
}

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
        
        // UE_LOG(LogTemp, Log, TEXT("[Enemy] Entered Range. Waiting for %.2fs"), CurrentWindupTime);
    }
}

bool AEnemyCharacter::IsFirstAttackWindupDone() const
{
    if (EnterRangeTime < 0.f) return true; // 에러 방지

    const float Now = GetWorld()->GetTimeSeconds();
    return (Now - EnterRangeTime) >= CurrentWindupTime;
}
