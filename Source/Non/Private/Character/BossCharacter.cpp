#include "Character/BossCharacter.h"
#include "Components/SphereComponent.h"
#include "Character/NonCharacterBase.h"
#include "Ability/NonAttributeSet.h"
#include "UI/InGameHUD.h"
#include "UObject/UObjectIterator.h"
#include "Data/BossDataAsset.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "TimerManager.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Effects/DamageNumberActor.h"
#include "GameplayEffect.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimInstance.h"
#include "Ability/NonAbilitySystemComponent.h"
#include "Ability/GA_EnemyTurn.h"
#include "Ability/GA_Boss_Grab.h"
#include "Ability/GA_Death.h"
#include "MotionWarpingComponent.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Animation/AnimSequence.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"


ABossCharacter::ABossCharacter()
{
    UIActivationZone = CreateDefaultSubobject<USphereComponent>(TEXT("UIActivationZone"));
    UIActivationZone->SetupAttachment(RootComponent);
    UIActivationZone->SetSphereRadius(2500.f);
    
    // [Fix] UI 인식용 거대 구체가 플레이어의 공격 트레이스를 가로막지 않도록 모든 충돌 채널을 무시합니다.
    UIActivationZone->SetCollisionResponseToAllChannels(ECR_Ignore);
    UIActivationZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly); 
    UIActivationZone->SetCollisionProfileName(TEXT("Trigger"));

    // 보스는 머리 위 HP바(Floating Widget)를 사용하지 않고 보스 전용 HUD UI를 사용하므로 컴포넌트를 제거합니다.
    if (HPBarWidget)
    {
        HPBarWidget->DestroyComponent();
        HPBarWidget = nullptr;
    }

    // 보스가 이동을 시작할 때 720도/초로 휙 돌아버리지 않도록 묵직한 회전 속도(180도/초) 적용
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->RotationRate = FRotator(0.f, 180.f, 0.f);
    }

    CurrentPhase = 1;
    bIsTransitioningPhase = false;
    BossData = nullptr;
}

void ABossCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABossCharacter, CurrentTargetActor);
}

void ABossCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->RotationRate = FRotator(0.f, 180.f, 0.f);
    }

    if (UIActivationZone)
    {
        UIActivationZone->OnComponentBeginOverlap.AddDynamic(this, &ABossCharacter::OnUIZoneOverlapBegin);
        UIActivationZone->OnComponentEndOverlap.AddDynamic(this, &ABossCharacter::OnUIZoneOverlapEnd);
    }

    // 블루프린트에서 GE_Damage가 누락되어 데미지 숫자 출력/피격 판정이 스킵되는 현상 방지
    if (!GE_Damage)
    {
        static UClass* LoadedGE = StaticLoadClass(UGameplayEffect::StaticClass(), nullptr, TEXT("/Game/Non/Blueprints/Abilities/GE/GE_Damage.GE_Damage_C"));
        if (LoadedGE)
        {
            GE_Damage = LoadedGE;
        }
    }

    // ★ 자식 블루프린트(BP_Boss_Golem 등)가 DefaultAbilities 배열을 오버라이드하여
    // 부모(BP_Boss_Base)의 GA_EnemyTurn이나 GA_Boss_Grab이 빠져 있더라도 서버에서 100% 자동 부여 보장!
    if (HasAuthority() && AbilitySystemComponent)
    {
        bool bHasTurn = false;
        bool bHasGrab = false;
        bool bHasDeath = false;
        for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
        {
            if (Spec.Ability)
            {
                if (Spec.Ability->IsA<UGA_EnemyTurn>()) bHasTurn = true;
                if (Spec.Ability->IsA<UGA_Boss_Grab>()) bHasGrab = true;
                if (Spec.Ability->IsA<UGA_Death>()) bHasDeath = true;
            }
        }
        if (!bHasTurn)
        {
            AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UGA_EnemyTurn::StaticClass(), 1, INDEX_NONE, this));
        }
        if (!bHasGrab && GrabAttemptMontage)
        {
            AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UGA_Boss_Grab::StaticClass(), 1, INDEX_NONE, this));
        }
        if (!bHasDeath)
        {
            static UClass* LoadedDeathBP = StaticLoadClass(UGameplayAbility::StaticClass(), nullptr, TEXT("/Game/Non/Blueprints/Abilities/GA/Enemy/GA_Enemy_Death.GA_Enemy_Death_C"));
            UClass* DeathClassToGive = LoadedDeathBP ? LoadedDeathBP : UGA_Death::StaticClass();
            AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(DeathClassToGive, 1, INDEX_NONE, this));
        }
    }

    // 블루프린트에서 수동으로 BossData를 넣었을 경우 동작하게끔 초기화
    if (BossData)
    {
        InitBossData(BossData);
    }
}

void ABossCharacter::UpdateHPBar() const
{
    // 원래 Enemy(오버헤드 UI, 타겟 프레임 등) 기능 상속 유지
    Super::UpdateHPBar();

    if (GetAttributeSet() && !bDied)
    {
        float CurHP = GetAttributeSet()->GetHP();
        float MaxHP = GetAttributeSet()->GetMaxHP();

        // 켜져 있는 HUD를 찾아 보스 체력 갱신 (C++ Widget 순회 방식)
        for (TObjectIterator<UInGameHUD> Itr; Itr; ++Itr)
        {
            if (Itr->GetWorld() == GetWorld())
            {
                Itr->UpdateBossHP(CurHP, MaxHP);
            }
        }

        // 2. 체력 변화에 따른 페이즈 검사
        const_cast<ABossCharacter*>(this)->CheckPhaseTransition();
    }
}

void ABossCharacter::ApplyDamageAt(float Amount, AActor* DamageInstigator, const FVector& WorldLocation, bool bIsCritical, FGameplayTag ReactionTag)
{
    // 페이즈 전환(연박) 중에는 완전 무적! 데미지를 무시하고 "무적" 텍스트 팝업을 띄웁니다.
    if (bIsTransitioningPhase)
    {
        if (DamageInstigator)
        {
            const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
            if (float* LastHitTime = RecentAttackerHitTimes.Find(DamageInstigator))
            {
                if ((CurrentTime - *LastHitTime) < 0.25f)
                {
                    return; // 0.25초 이내 동일 공격자의 연속 무적 텍스트 무시
                }
            }
            RecentAttackerHitTimes.Add(DamageInstigator, CurrentTime);
        }

        Multicast_SpawnImmuneText(WorldLocation);
        return;
    }

    // ── 보스 부위 파괴 데미지 처리
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        FName BestHitPart = MeshComp->FindClosestBone(WorldLocation);
        float MinDistSq = (BestHitPart != NAME_None) ? FVector::DistSquared(MeshComp->GetBoneLocation(BestHitPart), WorldLocation) : FLT_MAX;

        for (const TPair<FName, float>& Pair : PartHealthMap)
        {
            FName PartKey = Pair.Key;
            if (MeshComp->DoesSocketExist(PartKey))
            {
                FVector SocketLoc = MeshComp->GetSocketLocation(PartKey);
                float DistSq = FVector::DistSquared(SocketLoc, WorldLocation);
                if (DistSq < MinDistSq)
                {
                    MinDistSq = DistSq;
                    BestHitPart = PartKey;
                }
            }
        }

        if (BestHitPart != NAME_None)
        {
            ProcessPartDamage(BestHitPart, Amount);
        }
    }

    // 부위 파괴(Pain) 몽타주가 방금 발동되었거나 재생 중이라면 일반 피격 리액션(GA_HitReaction)이 덮어쓰지 않도록 ReactionTag 무시
    const FGameplayTag EffectiveReactionTag = bIsPlayingPartBreak ? FGameplayTag() : ReactionTag;
    Super::ApplyDamageAt(Amount, DamageInstigator, WorldLocation, bIsCritical, EffectiveReactionTag);

    // 보스 체력이 0 이하가 된 경우 사망 처리 이벤트 및 어빌리티 직접 발동 (3중 보장)
    if (AttributeSet && AttributeSet->GetHP() <= 0.f && !bDied)
    {
        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        {
            // 1) Effect.Death 게임플레이 이벤트 발송
            FGameplayTag DeathEventTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Death"), false);
            if (DeathEventTag.IsValid())
            {
                FGameplayEventData Payload;
                Payload.EventTag = DeathEventTag;
                Payload.Target = this;
                ASC->HandleGameplayEvent(DeathEventTag, &Payload);
            }

            // 2) Ability.Death 태그 직접 발동
            FGameplayTag DeathAbilityTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Death"), false);
            if (DeathAbilityTag.IsValid())
            {
                FGameplayTagContainer DeathTags;
                DeathTags.AddTag(DeathAbilityTag);
                ASC->TryActivateAbilitiesByTag(DeathTags);
            }

            // 3) UGA_Death 계열 어빌리티 인스턴스 직접 활성화 (CDO 태그 미등록 방어)
            for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
            {
                if (Spec.Ability && Spec.Ability->IsA<UGA_Death>() && !Spec.IsActive())
                {
                    ASC->TryActivateAbility(Spec.Handle);
                    break;
                }
            }
        }
    }
}

bool ABossCharacter::IsPlayingPartBreakMontage(const UAnimMontage* Montage) const
{
    if (!Montage) return false;

    for (const TPair<FName, UAnimMontage*>& Pair : PartBreakMontages)
    {
        if (Pair.Value == Montage)
        {
            return true;
        }
    }

    const FString MontageName = Montage->GetName();
    return MontageName.Contains(TEXT("Pain"), ESearchCase::IgnoreCase) ||
           MontageName.Contains(TEXT("Break"), ESearchCase::IgnoreCase) ||
           MontageName.Contains(TEXT("Groggy"), ESearchCase::IgnoreCase) ||
           MontageName.Contains(TEXT("Stun"), ESearchCase::IgnoreCase);
}

void ABossCharacter::OnPartBreakMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    EndPartBreakReaction();
}

void ABossCharacter::EndPartBreakReaction()
{
    if (!bIsPlayingPartBreak) return;
    bIsPlayingPartBreak = false;

    GetWorldTimerManager().ClearTimer(PartBreakTimerHandle);

    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(TEXT("State.HitReacting"), false);
        const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
        if (StunTag.IsValid()) ASC->RemoveLooseGameplayTag(StunTag);
        if (AttackTag.IsValid() && !bIsTransitioningPhase) ASC->RemoveLooseGameplayTag(AttackTag);
    }

    if (!IsDead() && !bIsTransitioningPhase)
    {
        if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
        {
            MoveComp->SetMovementMode(MOVE_Walking);
            MoveComp->bOrientRotationToMovement = true;
        }
    }
}

void ABossCharacter::ProcessPartDamage(FName BoneName, float Damage)
{
    if (PartHealthMap.Num() == 0 || Damage <= 0.f) return;

    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!MeshComp) return;

    // 1. 맞은 뼈나 소켓 이름부터 부모 본을 타고 올라가며 '파괴 가능 부위'인지 찾습니다.
    FName CurrentBone = BoneName;
    FName TargetPartBone = NAME_None;

    if (PartHealthMap.Contains(BoneName))
    {
        TargetPartBone = BoneName;
    }

    while (TargetPartBone == NAME_None && CurrentBone != NAME_None)
    {
        if (PartHealthMap.Contains(CurrentBone))
        {
            TargetPartBone = CurrentBone;
            break;
        }
        CurrentBone = MeshComp->GetParentBone(CurrentBone);
    }

    // 2. 등록된 부위를 찾았고, 아직 파괴되지 않았다면 데미지 적용
    if (TargetPartBone != NAME_None && !BrokenParts.Contains(TargetPartBone))
    {
        float& CurrentHealth = PartHealthMap[TargetPartBone];
        CurrentHealth -= Damage;

        // 3. 부위 파괴 판정
        if (CurrentHealth <= 0.f)
        {
            BrokenParts.Add(TargetPartBone);

            // 부위 파괴 영구 태그 부여
            if (FGameplayTag* FoundTag = PartBreakTags.Find(TargetPartBone))
            {
                if (FoundTag->IsValid() && GetAbilitySystemComponent())
                {
                    GetAbilitySystemComponent()->AddLooseGameplayTag(*FoundTag);
                }
            }

            // 서버 및 모든 클라이언트에 부위 파괴 연출/몽타주/회전 잠금 동기화 전파 (Multicast가 서버에서도 OnPartBroken을 자동 실행함)
            if (HasAuthority())
            {
                Multicast_OnPartBroken(TargetPartBone);
            }
            else
            {
                OnPartBroken(TargetPartBone);
            }
        }
    }
}

void ABossCharacter::InitBossData(const UBossDataAsset* InBossData)
{
    InitBossPhaseData(InBossData);
}

void ABossCharacter::InitBossPhaseData(const UBossDataAsset* InBossPhaseData)
{
    if (InBossPhaseData)
    {
        BossData = InBossPhaseData;
        CurrentPhase = 1;
        ApplyPhase(1); // 1페이즈 스킬 즉시 지급
    }
}

void ABossCharacter::CheckPhaseTransition()
{
    if (!GetAttributeSet() || bDied || bIsTransitioningPhase) return;
    if (!BossData || BossData->PhaseList.Num() == 0) return;
    
    // 다음 페이즈가 있는지 확인
    if (CurrentPhase >= BossData->PhaseList.Num()) return;

    float HealthPercent = GetAttributeSet()->GetHP() / FMath::Max(1.f, GetAttributeSet()->GetMaxHP());

    // 현재 페이즈 기준 하한선
    float CurrentThreshold = BossData->PhaseList[CurrentPhase - 1].HealthThresholdPercent;

    if (HealthPercent <= CurrentThreshold)
    {
        ApplyPhase(CurrentPhase + 1);
    }
}

void ABossCharacter::ApplyPhase_Implementation(int32 TargetPhase)
{
    if (!BossData || !GetAbilitySystemComponent()) return;
    
    CurrentPhase = TargetPhase;
    bIsTransitioningPhase = true; // 무적 시작

    // 페이즈 전환 몽타주 도중 보스가 플레이어를 따라오거나 회전하지 않도록 이동 및 회전 완전 잠금!
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        AIC->ClearFocus(EAIFocusPriority::Gameplay);
        AIC->StopMovement();
    }
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->StopMovementImmediately();
        MoveComp->bOrientRotationToMovement = false;
        MoveComp->DisableMovement(); // 몽타주 동안 위치 및 이동 완전 차단!
    }

    // 페이즈 전환 동안 State.Attacking 태그 주입하여 턴 어빌리티 및 타 공격 어빌리티 100% 차단!
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
        if (AttackTag.IsValid())
        {
            ASC->AddLooseGameplayTag(AttackTag);
        }
    }
    
    // 1. 기존 페이즈 스킬들 제거 (서버 권한에서만 수행)
    if (HasAuthority() && GetAbilitySystemComponent())
    {
        for (FGameplayAbilitySpecHandle Handle : PhaseAbilityHandles)
        {
            GetAbilitySystemComponent()->ClearAbility(Handle);
        }
        PhaseAbilityHandles.Empty();
    }

    // 새 페이즈 데이터 가져오기
    if (BossData->PhaseList.IsValidIndex(CurrentPhase - 1))
    {
        const FBossPhaseData& PhaseData = BossData->PhaseList[CurrentPhase - 1];

        // 2. 몽타주 재생 및 몽타주 완전 종료 델리게이트 바인딩 (초 단위 계산 0개!)
        if (PhaseData.bUseTransitionMontage && PhaseData.TransitionMontage)
        {
            if (HasAuthority())
            {
                Multicast_PlayBossSkillMontage(PhaseData.TransitionMontage, 1.0f);
            }
            float PlayTime = PlayAnimMontage(PhaseData.TransitionMontage);
            if (PlayTime > 0.0f)
            {
                if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
                {
                    FOnMontageEnded EndDelegate;
                    EndDelegate.BindUObject(this, &ABossCharacter::OnPhaseTransitionMontageEnded);
                    AnimInstance->Montage_SetEndDelegate(EndDelegate, PhaseData.TransitionMontage);
                }
            }
            else
            {
                EndPhaseTransition();
            }
        }
        else
        {
            EndPhaseTransition();
        }

        // 3. 새 스킬 지급 (서버 권한에서만 수행)
        if (HasAuthority() && GetAbilitySystemComponent())
        {
            for (TSubclassOf<UGameplayAbility> AbilityClass : PhaseData.GrantedSkills)
            {
                if (AbilityClass)
                {
                    FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
                    FGameplayAbilitySpecHandle Handle = GetAbilitySystemComponent()->GiveAbility(Spec);
                    PhaseAbilityHandles.Add(Handle);
                }
            }
        }
    }
    else
    {
         EndPhaseTransition();
    }

    OnBossPhaseChanged.Broadcast(CurrentPhase);
}

void ABossCharacter::OnPhaseTransitionMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    EndPhaseTransition();
}

void ABossCharacter::EndPhaseTransition()
{
    bIsTransitioningPhase = false; // 무적 해제, 정상 전투 시작

    // 페이즈 전환 완료 시 State.Attacking 태그 C++ 제거!
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
        if (AttackTag.IsValid())
        {
            ASC->RemoveLooseGameplayTag(AttackTag);
        }
    }

    // 페이즈 전환이 마쳤으므로 보스 이동 및 회전 정상 복구!
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->SetMovementMode(MOVE_Walking);
        MoveComp->bOrientRotationToMovement = true;
    }
}

void ABossCharacter::OnUIZoneOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                                  bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor && OtherActor != this && !bDied)
    {
        if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(OtherActor))
        {
            for (TObjectIterator<UInGameHUD> Itr; Itr; ++Itr)
            {
                if (Itr->GetWorld() == GetWorld())
                {
                    float MaxHP = GetAttributeSet() ? GetAttributeSet()->GetMaxHP() : 1.f;
                    float CurHP = GetAttributeSet() ? GetAttributeSet()->GetHP() : 1.f;
                    Itr->ShowBossFrame(true, GetEnemyName(), CurHP, MaxHP);
                }
            }
        }
    }
}

void ABossCharacter::OnUIZoneOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (OtherActor && OtherActor != this && !bDied)
    {
        if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(OtherActor))
        {
            for (TObjectIterator<UInGameHUD> Itr; Itr; ++Itr)
            {
                if (Itr->GetWorld() == GetWorld())
                {
                    Itr->ShowBossFrame(false);
                }
            }
        }
    }
}

void ABossCharacter::Multicast_SpawnDamageNumber_Implementation(float Amount, FVector WorldLocation, bool bIsCritical)
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
    
    // 블루프린트에서 세팅 누락된 경우 기본 적 데미지 위젯을 로드합니다.
    if (!SpawnClass)
    {
        static UClass* LoadedClass = StaticLoadClass(ADamageNumberActor::StaticClass(), nullptr, TEXT("/Game/Non/Blueprints/Combat/FX/BP_DamageNumberActor_Enemy.BP_DamageNumberActor_Enemy_C"));
        if (LoadedClass)
        {
            SpawnClass = LoadedClass;
        }
        else
        {
            SpawnClass = ADamageNumberActor::StaticClass();
        }
    }

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SP.Owner = this;

    // [Final] 이제 UIActivationZone이 공격을 막지 않으므로, 정확한 타격 지점(WorldLocation)을 그대로 사용합니다.
    FVector SpawnLoc = WorldLocation;

    if (ADamageNumberActor* A = GetWorld()->SpawnActor<ADamageNumberActor>(SpawnClass, SpawnLoc, FRotator::ZeroRotator, SP))
    {
        A->InitWithFlags(Amount, bIsCritical);
    }
}

void ABossCharacter::OnPartBroken_Implementation(FName BoneName)
{
    // 0. 진행 중이던 공격/턴 어빌리티 및 히트박스 즉시 취소하고, 이동/회전을 완벽히 잠급니다!
    AttackHitbox_Disable();

    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        ASC->CancelAllAbilities();
    }

    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        AIC->ClearFocus(EAIFocusPriority::Gameplay);
        AIC->StopMovement();
    }

    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->StopMovementImmediately();
        MoveComp->bOrientRotationToMovement = false;
    }

    // 1. 부위 파괴(Pain) 몽타주 재생 및 회전/행동 잠금 유지
    if (UAnimMontage** FoundMontage = PartBreakMontages.Find(BoneName))
    {
        if (*FoundMontage)
        {
            bIsPlayingPartBreak = true;

            if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
            {
                MoveComp->DisableMovement();
            }

            if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
            {
                const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(TEXT("State.HitReacting"), false);
                const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"), false);
                if (StunTag.IsValid()) ASC->AddLooseGameplayTag(StunTag);
                if (AttackTag.IsValid()) ASC->AddLooseGameplayTag(AttackTag);
            }

            const float PlayTime = PlayAnimMontage(*FoundMontage);
            if (PlayTime > 0.0f)
            {
                if (USkeletalMeshComponent* MeshComp = GetMesh())
                {
                    if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
                    {
                        FOnMontageEnded EndDelegate;
                        EndDelegate.BindUObject(this, &ABossCharacter::OnPartBreakMontageEnded);
                        AnimInstance->Montage_SetEndDelegate(EndDelegate, *FoundMontage);
                    }
                }
                GetWorldTimerManager().SetTimer(PartBreakTimerHandle, this, &ABossCharacter::EndPartBreakReaction, PlayTime, false);
            }
            else
            {
                GetWorldTimerManager().SetTimer(PartBreakTimerHandle, this, &ABossCharacter::EndPartBreakReaction, 2.0f, false);
            }
        }
    }

    // 2. 부위 메쉬(보석/코어 등) 숨김 처리
    TArray<USceneComponent*> AllSceneComps;
    GetComponents<USceneComponent>(AllSceneComps);

    for (USceneComponent* Comp : AllSceneComps)
    {
        if (Comp)
        {
            FName AttachSocket = Comp->GetAttachSocketName();
            FString CompName = Comp->GetName();

            if (AttachSocket == BoneName || CompName.Contains(BoneName.ToString()) || AttachSocket.ToString().Contains(TEXT("Heart")) || CompName.Contains(TEXT("Heart")) || CompName.Contains(TEXT("Gem")))
            {
                Comp->SetVisibility(false, true);
                Comp->SetHiddenInGame(true, true);

                if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
                {
                    PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
            }
        }
    }
}

void ABossCharacter::Multicast_OnPartBroken_Implementation(FName BoneName)
{
    OnPartBroken(BoneName);
}

bool ABossCharacter::IsBossSkillOnCooldown(FName SkillName) const
{
    if (SkillName.IsNone()) return false;
    if (const double* ExpireTime = SkillCooldownMap.Find(SkillName))
    {
        return FPlatformTime::Seconds() < *ExpireTime;
    }
    return false;
}

void ABossCharacter::SetBossSkillCooldown(FName SkillName, float CooldownSeconds)
{
    if (SkillName.IsNone() || CooldownSeconds <= 0.0f) return;
    SkillCooldownMap.Add(SkillName, FPlatformTime::Seconds() + CooldownSeconds);
}

void ABossCharacter::Multicast_PlayBossSkillMontage_Implementation(UAnimMontage* Montage, float PlayRate)
{
    if (Montage)
    {
        // ★ 몽타주에 MotionWarping 노티파이가 들어 있는데 원본 AnimSequence(예: A_Golem_Atk1)에 bEnableRootMotion 체크가 빠져 있는 경우,
        // 런타임에 자동으로 bEnableRootMotion을 활성화하여 점프/회전 모션 워핑이 100% 작동하도록 보장!
        bool bHasMotionWarpingNotify = false;
        for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
        {
            if (NotifyEvent.NotifyStateClass && NotifyEvent.NotifyStateClass->IsA<UAnimNotifyState_MotionWarping>())
            {
                bHasMotionWarpingNotify = true;
                break;
            }
        }
        if (bHasMotionWarpingNotify)
        {
            for (FSlotAnimationTrack& SlotTrack : Montage->SlotAnimTracks)
            {
                for (FAnimSegment& Segment : SlotTrack.AnimTrack.AnimSegments)
                {
                    if (UAnimSequence* AnimSeq = Cast<UAnimSequence>(Segment.GetAnimReference()))
                    {
                        if (!AnimSeq->bEnableRootMotion)
                        {
                            AnimSeq->bEnableRootMotion = true;
                        }
                    }
                }
            }
        }

        if (USkeletalMeshComponent* MeshComp = GetMesh())
        {
            if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
            {
                if (AnimInst->Montage_IsPlaying(Montage))
                {
                    return;
                }
            }
        }
        PlayAnimMontage(Montage, (PlayRate > 0.0f) ? PlayRate : 1.0f);
    }
}

void ABossCharacter::UpdateBossMotionWarpTarget(AActor* TargetActorOverride)
{
    AActor* TargetActor = TargetActorOverride ? TargetActorOverride : CurrentTargetActor.Get();

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

    if (!TargetActor && GetWorld())
    {
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            TargetActor = PC->GetPawn();
        }
    }

    if (!TargetActor) return;

    if (HasAuthority() && CurrentTargetActor != TargetActor)
    {
        CurrentTargetActor = TargetActor;
    }

    const FVector BossLoc = GetActorLocation();
    const FVector PlayerLoc = TargetActor->GetActorLocation();
    FVector DirToPlayer = (PlayerLoc - BossLoc);
    DirToPlayer.Z = 0.0f;
    const float Dist2D = DirToPlayer.Size2D();
    const FVector DirNorm = (Dist2D > KINDA_SMALL_NUMBER) ? (DirToPlayer / Dist2D) : GetActorForwardVector().GetSafeNormal2D();

    // 플레이어 머리 위를 덮치거나 캡슐끼리 겹쳐 끼이지 않도록 플레이어 정면 살짝 앞(약 90cm 앞)을 착지 지점으로 설정하되,
    // 너무 가까울 때(220cm 이내) 점프 모션이 제자리 뜀박질로 찌그러지지 않도록 최소 전진 거리를 확보합니다.
    float DesiredDist = FMath::Max(Dist2D - 90.0f, FMath::Min(Dist2D, 180.0f));
    FVector TargetLoc = BossLoc + DirNorm * DesiredDist;
    TargetLoc.Z = PlayerLoc.Z;

    FRotator TargetRot = DirNorm.Rotation();
    TargetRot.Pitch = 0.0f;
    TargetRot.Roll = 0.0f;

    if (HasAuthority())
    {
        Multicast_UpdateMotionWarpTarget(FName("LocationTarget"), TargetLoc, TargetRot);
    }
    else if (UMotionWarpingComponent* MWComp = FindComponentByClass<UMotionWarpingComponent>())
    {
        MWComp->AddOrUpdateWarpTargetFromLocationAndRotation(FName("LocationTarget"), TargetLoc, TargetRot);
    }
}

void ABossCharacter::Multicast_UpdateMotionWarpTarget_Implementation(FName WarpTargetName, FVector TargetLoc, FRotator TargetRot)
{
    if (UMotionWarpingComponent* MWComp = FindComponentByClass<UMotionWarpingComponent>())
    {
        const FName EffectiveName = WarpTargetName.IsNone() ? FName("LocationTarget") : WarpTargetName;
        MWComp->AddOrUpdateWarpTargetFromLocationAndRotation(EffectiveName, TargetLoc, TargetRot);
    }
}

void ABossCharacter::Multicast_PlayBossTurnMontage_Implementation(UAnimMontage* Montage, FRotator TargetRotation, FName WarpTargetName)
{
    if (UMotionWarpingComponent* MWComp = FindComponentByClass<UMotionWarpingComponent>())
    {
        const FName EffectiveWarpName = WarpTargetName.IsNone() ? FName("RotateTarget") : WarpTargetName;
        MWComp->AddOrUpdateWarpTargetFromTransform(EffectiveWarpName, FTransform(TargetRotation, GetActorLocation()));
    }

    if (Montage)
    {
        if (USkeletalMeshComponent* MeshComp = GetMesh())
        {
            if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
            {
                if (AnimInst->Montage_IsPlaying(Montage))
                {
                    return;
                }
            }
        }
        PlayAnimMontage(Montage, 1.0f);
    }
}

