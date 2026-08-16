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

    CurrentPhase = 1;
    bIsTransitioningPhase = false;
    BossData = nullptr;
}

void ABossCharacter::BeginPlay()
{
    Super::BeginPlay();

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
    // 페이즈 전환(연박) 중에는 완전 무적! 데미지를 무시합니다.
    if (bIsTransitioningPhase)
    {
        return;
    }

    // ── 보스 부위 파괴 데미지 처리
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        FName HitBone = MeshComp->FindClosestBone(WorldLocation);
        if (HitBone != NAME_None)
        {
            ProcessPartDamage(HitBone, Amount);
        }
    }

    Super::ApplyDamageAt(Amount, DamageInstigator, WorldLocation, bIsCritical, ReactionTag);
}

void ABossCharacter::ProcessPartDamage(FName BoneName, float Damage)
{
    if (PartHealthMap.Num() == 0 || Damage <= 0.f) return;

    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!MeshComp) return;

    // 1. 맞은 뼈부터 부모를 타고 올라가며 에디터에 등록된 '파괴 가능 부위'인지 찾습니다.
    FName CurrentBone = BoneName;
    FName TargetPartBone = NAME_None;

    while (CurrentBone != NAME_None)
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
            
            // 블루프린트 이벤트 호출 (이펙트, 메쉬 숨기기 등을 여기서 처리하세요!)
            OnPartBroken(TargetPartBone);

            // 부위 파괴 몽타주 재생
            if (UAnimMontage** FoundMontage = PartBreakMontages.Find(TargetPartBone))
            {
                if (*FoundMontage)
                {
                    PlayAnimMontage(*FoundMontage);

                    // ── [추가] AI 일시 정지를 위한 태그 부여 ──
                    // 보스에게 '피격 상태' 태그를 부여합니다.
                    const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(TEXT("State.HitReacting"));
                    if (GetAbilitySystemComponent())
                    {
                        GetAbilitySystemComponent()->AddLooseGameplayTag(StunTag);
                    }

                    // 2초 뒤에 태그를 다시 제거하는 타이머
                    FTimerHandle StunTimerHandle;
                    GetWorldTimerManager().SetTimer(StunTimerHandle, [this, StunTag]()
                    {
                        if (GetAbilitySystemComponent())
                        {
                            GetAbilitySystemComponent()->RemoveLooseGameplayTag(StunTag);
                        }
                    }, 2.0f, false);
                }
            }

            // 부위 파괴 영구 태그 부여
            if (FGameplayTag* FoundTag = PartBreakTags.Find(TargetPartBone))
            {
                if (FoundTag->IsValid())
                {
                    if (GetAbilitySystemComponent())
                    {
                        GetAbilitySystemComponent()->AddLooseGameplayTag(*FoundTag);
                    }
                }
            }

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, 
                    FString::Printf(TEXT("BOSS PART BROKEN: %s"), *TargetPartBone.ToString()));
            }
        }
    }
}

void ABossCharacter::InitBossData(const UBossDataAsset* InBossData)
{
    if (InBossData)
    {
        InitFromDataAsset(InBossData); // 부모 클래스의 기본 초기화 (체력, 공격력 등)
        BossData = InBossData;
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
    
    // 1. 기존 페이즈 스킬들 제거
    for (FGameplayAbilitySpecHandle Handle : PhaseAbilityHandles)
    {
        GetAbilitySystemComponent()->ClearAbility(Handle);
    }
    PhaseAbilityHandles.Empty();

    // 새 페이즈 데이터 가져오기
    if (BossData->PhaseList.IsValidIndex(CurrentPhase - 1))
    {
        const FBossPhaseData& PhaseData = BossData->PhaseList[CurrentPhase - 1];

        // 2. 몽타주 재생 및 몽타주 완전 종료 델리게이트 바인딩 (초 단위 계산 0개!)
        if (PhaseData.TransitionMontage)
        {
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

        // 3. 새 스킬 지급
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
    if (!GetWorld()) return;

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
