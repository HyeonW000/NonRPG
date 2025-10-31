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
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Engine/World.h"

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
    InitializeAttributes();
    BindAttributeDelegates();

    if (HPBarWidget)
    {
        HPBarWidget->InitWidget();
        UpdateHPBar();
        HPBarWidget->SetVisibility(false);
    }

    if (AnimSet)
    {
        if (AnimSet->HitReact_F) HitReact_F = AnimSet->HitReact_F;
        if (AnimSet->HitReact_B) HitReact_B = AnimSet->HitReact_B;
        if (AnimSet->HitReact_L) HitReact_L = AnimSet->HitReact_L;
        if (AnimSet->HitReact_R) HitReact_R = AnimSet->HitReact_R;

        // Death 설정은 DataAsset 기준으로 항상 덮어쓰기
        bUseDeathMontage = AnimSet->bUseDeathMontage;
        DeathMontage = AnimSet->DeathMontage;
    }

    if (bUseSpawnFadeIn)
    {
        InitSpawnFadeMIDs();
        PlaySpawnFadeIn(SpawnFadeDuration);
    }
}

void AEnemyCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
}

void AEnemyCharacter::InitializeAttributes()
{
    if (AbilitySystemComponent && GE_DefaultAttributes)
    {
        FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
        Ctx.AddSourceObject(this);

        if (FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(GE_DefaultAttributes, 1.f, Ctx);
            Spec.IsValid())
        {
            AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        }
    }
    UpdateHPBar();
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

    OnEnemyDied.Broadcast(this);

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->DisableMovement();
        Move->StopMovementImmediately();
    }
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AddDeadTag();

    if (bUseDeathMontage && DeathMontage && GetMesh())
    {
        const float Len = PlayAnimMontage(DeathMontage, 1.0f);
        if (HPBarWidget) HPBarWidget->SetVisibility(false);
        SetLifeSpan(FMath::Max(1.0f, Len + 0.2f));
        return;
    }

    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        Skel->SetCollisionProfileName(FName("Ragdoll"));
        Skel->SetAllBodiesSimulatePhysics(true);
        Skel->SetSimulatePhysics(true);
        Skel->WakeAllRigidBodies();
    }
    if (HPBarWidget) HPBarWidget->SetVisibility(false);
    SetLifeSpan(5.f);
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

void AEnemyCharacter::ApplyDamageAt(float Amount, AActor* DamageInstigator, const FVector& WorldLocation)
{
    if (Amount <= 0.f || !AbilitySystemComponent || !AttributeSet) return;
    if (IsDead()) return;

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
            AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        }
    }
    else
    {
        ApplyHealthDelta_Direct(-Amount);
    }

    // ── 데미지 숫자
    Multicast_SpawnDamageNumber(Amount, WorldLocation, /*bIsCritical=*/false);

    // ── 피격 리액션
    OnGotHit(Amount, DamageInstigator, WorldLocation);

    // ── ★ Reactive 어그로: "맞아서 어그로" 플래그 + 타임스탬프 기록 ★
    if (AggroStyle == EAggroStyle::Reactive)
    {
        MarkAggroByHit(DamageInstigator);   // bAggroByHit = true, LastAggroByHitTime 업데이트
        SetAggro(true);
        UE_LOG(LogTemp, Verbose, TEXT("[Enemy] ApplyDamageAt: Reactive hit → MarkAggroByHit() & SetAggro(TRUE)"));
    }

    // 전투 상태 진입(HP바 표시 등)
    EnterCombat();
}

void AEnemyCharacter::MarkAggroByHit(AActor* InstigatorActor)
{
    // 누가 때렸는지까지 쓰고 싶으면 여기서 저장/검증 가능
    bAggroByHit = true;
    LastAggroByHitTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    UE_LOG(LogTemp, Verbose, TEXT("[Aggro] MarkAggroByHit by %s (Hold=%.2fs)"),
        *GetNameSafe(InstigatorActor), AggroByHitHoldTime);
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

    const float Cur = AttributeSet ? AttributeSet->GetHP() : 0.f;
    const float Max = AttributeSet ? AttributeSet->GetMaxHP() : 1.f;

    bool bShouldShow = true;
    if (bShowHPBarOnlyInCombat)
    {
        bShouldShow = bInCombat;
    }
    else
    {
        bShouldShow = (Cur < Max - KINDA_SMALL_NUMBER);
    }
    HPBarWidget->SetVisibility(bShouldShow);
}

void AEnemyCharacter::SetAggro(bool bNewAggro)
{
    bAggro = bNewAggro;
}

void AEnemyCharacter::TryStartAttack()
{
    EnterCombat();

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

void AEnemyCharacter::OnGotHit(float Damage, AActor* InstigatorActor, const FVector& ImpactPoint)
{
    ApplyHitStopSelf(0.2f, 0.06f);

    if (!bCanHitReact || IsDead() || Damage <= 0.f)
    {
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
    UAnimMontage* M = nullptr;
    switch (Quad)
    {
    case EHitQuadrant::Front: M = HitReact_F; break;
    case EHitQuadrant::Back:  M = HitReact_B; break;
    case EHitQuadrant::Left:  M = HitReact_L; break;
    case EHitQuadrant::Right: M = HitReact_R; break;
    }
    if (!M) M = HitReact_F;
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
    if (!Other || Other == this) return;
    if (HitOnce.Contains(Other)) return;

    // 플레이어만 맞추기
    if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(Other))
    {
        HitOnce.Add(Other);

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

        // 디버그 찍기
        UE_LOG(LogTemp, Warning, TEXT("[Hitbox] %s -> %s Impact=%s  FromSweep=%d  FinalLoc=%s"),
            *GetName(), *GetNameSafe(Other), *Sweep.ImpactPoint.ToString(), bFromSweep ? 1 : 0, *HitLoc.ToString());
        if (UWorld* W = GetWorld())
        {
            DrawDebugSphere(W, HitLoc, 10.f, 12, FColor::Yellow, false, 2.0f);
        }

        // 3) 실제 데미지 적용
        const float Damage = 15.f; // TODO: DataAsset/스탯/난이도에 따라
        Player->ApplyDamageAt(Damage, this, HitLoc);
    }
}

void AEnemyCharacter::BlockAttackFor(float Seconds)
{
    if (UWorld* W = GetWorld())
    {
        NextAttackAllowedTime = FMath::Max(NextAttackAllowedTime, W->GetTimeSeconds() + FMath::Max(0.f, Seconds));
    }
}

bool AEnemyCharacter::IsAttackAllowed() const
{
    if (IsDead()) return false;
    if (const UWorld* W = GetWorld())
    {
        return W->GetTimeSeconds() >= NextAttackAllowedTime;
    }
    return true;
}

void AEnemyCharacter::StartHitMovePause()
{
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        // 현재 속도 저장 후, 감속
        SavedMaxWalkSpeed = Move->MaxWalkSpeed;
        Move->MaxWalkSpeed = SavedMaxWalkSpeed * HitMoveSpeedScale;

        // 완전 정지 느낌 원하면 즉시 멈춤 + 브레이킹 강화
        if (HitMoveSpeedScale <= KINDA_SMALL_NUMBER)
        {
            Move->StopMovementImmediately();
            Move->BrakingFrictionFactor = 4.0f; // 잠깐 급브레이크
        }
    }

    // AI 이동도 잠깐 멈춤
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        AIC->StopMovement();
    }

    GetWorldTimerManager().ClearTimer(HitMovePauseTimer);
    GetWorldTimerManager().SetTimer(HitMovePauseTimer, this, &AEnemyCharacter::EndHitMovePause, HitMovePauseDuration, false);
}

void AEnemyCharacter::EndHitMovePause()
{
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->MaxWalkSpeed = (SavedMaxWalkSpeed > 0.f) ? SavedMaxWalkSpeed : Move->MaxWalkSpeed;
        Move->BrakingFrictionFactor = 1.0f; // 원복
    }
}

void AEnemyCharacter::MarkEnteredAttackRange()
{
    if (const UWorld* W = GetWorld())
    {
        EnterRangeTime = W->GetTimeSeconds();

        // 웜업 시간 동안은 공격 금지도 함께 걸어주면 더 확실
        BlockAttackFor(FirstAttackWindup);
    }
}

bool AEnemyCharacter::IsFirstAttackWindupDone() const
{
    if (FirstAttackWindup <= 0.f) return true;       // 웜업이 0이면 항상 통과
    if (EnterRangeTime < 0.f)     return false;      // 기록 없음 = 아직 웜업 시작 전

    if (const UWorld* W = GetWorld())
    {
        return (W->GetTimeSeconds() - EnterRangeTime) >= FirstAttackWindup;
    }
    return false;
}

//Fade
void AEnemyCharacter::InitSpawnFadeMIDs()
{
    SpawnFadeMIDs.Reset();

    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        const int32 Num = Skel->GetNumMaterials();
        for (int32 i = 0; i < Num; ++i)
        {
            if (UMaterialInterface* Mat = Skel->GetMaterial(i))
            {
                UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
                if (MID)
                {
                    MID->SetScalarParameterValue(FadeParamName, 0.0f); // 처음엔 안 보이게
                    Skel->SetMaterial(i, MID);
                    SpawnFadeMIDs.Add(MID);
                }
            }
        }
    }
}

void AEnemyCharacter::PlaySpawnFadeIn(float Duration)
{
    const float UseLen = (Duration > 0.f) ? Duration : SpawnFadeDuration;

    // MID 없으면 만들어두고 페이드값 0으로
    for (UMaterialInstanceDynamic* MID : SpawnFadeMIDs)
    {
        if (MID) MID->SetScalarParameterValue(FadeParamName, 0.0f);
    }

    BeginSpawnFade(UseLen);
}

void AEnemyCharacter::BeginSpawnFade(float DurationSeconds)
{
    SpawnFadeLen = FMath::Max(0.01f, DurationSeconds);
    SpawnFadeStartTime = GetWorld()->GetTimeSeconds();
    bSpawnFadeActive = true;

    if (bPauseAIWhileFading)
    {
        if (UCharacterMovementComponent* Move = GetCharacterMovement())
        {
            SavedMaxWalkSpeed = Move->MaxWalkSpeed;
            Move->DisableMovement();
        }
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            if (UBrainComponent* Brain = AIC->GetBrainComponent())
            {
                Brain->StopLogic(TEXT("SpawnFade"));
            }
        }
    }

    GetWorldTimerManager().ClearTimer(SpawnFadeTimer);
    GetWorldTimerManager().SetTimer(SpawnFadeTimer, this, &AEnemyCharacter::TickSpawnFade, 0.016f, true);
}

void AEnemyCharacter::TickSpawnFade()
{
    if (!bSpawnFadeActive) return;

    const float Now = GetWorld()->GetTimeSeconds();
    float T = (Now - SpawnFadeStartTime) / FMath::Max(0.01f, SpawnFadeLen);
    T = FMath::Clamp(T, 0.f, 1.f);

    // 페이드 값 적용
    for (UMaterialInstanceDynamic* MID : SpawnFadeMIDs)
    {
        if (MID) MID->SetScalarParameterValue(FadeParamName, T);
    }

    if (T >= 1.f)
    {
        bSpawnFadeActive = false;
        OnSpawnFadeFinished();
    }
}

// 페이드 종료 시 이동/AI 재개
void AEnemyCharacter::OnSpawnFadeFinished()
{
    GetWorldTimerManager().ClearTimer(SpawnFadeTimer);

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->SetMovementMode(MOVE_Walking);
        if (SavedMaxWalkSpeed > 0.f)
        {
            Move->MaxWalkSpeed = SavedMaxWalkSpeed;
        }
    }

    if (bAutoResumeAfterFade)
    {
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            if (UBrainComponent* Brain = AIC->GetBrainComponent())
            {
                Brain->StartLogic();
            }
        }
    }
}