#include "Combat/DamageAOE.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Components/SceneComponent.h"
#include "Character/EnemyCharacter.h"
#include "Character/NonCharacterBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

ADamageAOE::ADamageAOE()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);
}

void ADamageAOE::ConfigureBox(FVector InExtent, float InDamage, float InDuration, float InInterval, bool bSingleHit)
{
    Shape = EAOEShape::Box;
    BoxExtent = InExtent;
    Damage = InDamage;
    Duration = InDuration;
    TickInterval = InInterval;
    bSingleHitPerActor = bSingleHit;
}

void ADamageAOE::ConfigureSphere(float InRadius, float InDamage, float InDuration, float InInterval, bool bSingleHit)
{
    Shape = EAOEShape::Sphere;
    Radius = InRadius;
    Damage = InDamage;
    Duration = InDuration;
    TickInterval = InInterval;
    bSingleHitPerActor = bSingleHit;
}

// Configure capsule shape (half-height + radius)
void ADamageAOE::ConfigureCapsule(float InHalfHeight, float InRadius, float InDamage, float InDuration, float InInterval, bool bSingleHit)
{
    Shape = EAOEShape::Capsule;
    CapsuleHalfHeight = InHalfHeight;
    Radius = InRadius; // reuse Radius for capsule radius
    Damage = InDamage;
    Duration = InDuration;
    TickInterval = InInterval;
    bSingleHitPerActor = bSingleHit;
}

void ADamageAOE::BeginPlay()
{
    Super::BeginPlay();

    if (bServerOnly && !HasAuthority())
    {
        SetLifeSpan(Duration + SpawnDelay);
        return;
    }

    // 소켓 따라가기 설정
    if (bAttachToOwnerSocket && GetOwner())
    {
        USceneComponent* Base = nullptr;
        if (const ACharacter* C = Cast<ACharacter>(GetOwner()))
        {
            Base = C->GetMesh();
        }
        if (!Base) Base = GetOwner()->GetRootComponent();

        if (Base)
        {
            if (AttachSocketName != NAME_None && Base->DoesSocketExist(AttachSocketName))
            {
                AttachToComponent(Base, FAttachmentTransformRules::KeepWorldTransform, AttachSocketName);
            }
            else
            {
                AttachToComponent(Base, FAttachmentTransformRules::KeepWorldTransform);
            }
            FollowComp = Base;
        }
    }

    // 격발 딜레이 설정
    if (SpawnDelay > 0.f)
    {
        GetWorldTimerManager().SetTimer(StartDelayTimer, this, &ADamageAOE::StartDamageActive, SpawnDelay, false);
        SetLifeSpan(Duration + SpawnDelay);
    }
    else
    {
        StartDamageActive();
        SetLifeSpan(Duration);
    }
}

void ADamageAOE::StartDamageActive()
{
    // 첫 타격 실행
    DoHit();

    // 반복 실행 설정
    if (TickInterval > 0.001f)
    {
        GetWorldTimerManager().SetTimer(TickTimer, this, &ADamageAOE::DoHit, TickInterval, true);
    }

    // C++ 폭발 이펙트 & 사운드 처리
    if (GetWorld())
    {
        const FVector SpawnLoc = GetActorLocation();
        const FRotator SpawnRot = GetActorRotation();

        // Niagara 이펙트 스폰
        if (TriggerNiagaraEffect)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), TriggerNiagaraEffect, SpawnLoc, SpawnRot, TriggerEffectScale);
        }

        // 사운드 재생
        if (TriggerSound)
        {
            UGameplayStatics::PlaySoundAtLocation(GetWorld(), TriggerSound, SpawnLoc);
        }
    }

    // 블루프린트에서 폭발/충격파 이펙트 및 사운드 동기화를 위한 이벤트 호출
    OnAOEActivated();
}

FTransform ADamageAOE::ResolveTransform() const
{
    if (FollowComp.IsValid())
    {
        FTransform SocketTrans;
        bool bFoundSocket = (AttachSocketName != NAME_None && FollowComp->DoesSocketExist(AttachSocketName));
        
        if (bFoundSocket)
            SocketTrans = FollowComp->GetSocketTransform(AttachSocketName);
        else
            SocketTrans = FollowComp->GetComponentTransform();

        FTransform OffsetTrans(RelativeRotationOffset, RelativeOffset);
        return OffsetTrans * SocketTrans;
    }
    else
    {
        // 따라가지 않을 때: 현재 액터 트랜스폼 + 로컬 오프셋 적용
        // (보통 스폰 시점에 위치를 잡지만, 추가 오프셋이 있다면 적용)
        FTransform ActorTrans = GetActorTransform();
        FTransform OffsetTrans(RelativeRotationOffset, RelativeOffset);
        return OffsetTrans * ActorTrans;
    }
}

bool ADamageAOE::IsValidTarget(AActor* Other) const
{
    if (!Other || Other == this || Other == GetOwner()) return false;

    switch (Team)
    {
    case ETeamSideAOE::Enemy:
        // 적군이 쏜 것 -> 타겟은 플레이어
        if (Cast<ANonCharacterBase>(Other)) return true;
        break;
        
    case ETeamSideAOE::Player:
        // 플레이어가 쏜 것 -> 타겟은 Enemy
        if (Cast<AEnemyCharacter>(Other)) return true;
        break;
        
    case ETeamSideAOE::Neutral:
        return true;
    }

    return false;
}

void ADamageAOE::ApplyDamageTo(AActor* Other, const FVector& HitPoint)
{
    if (!Other) return;

    AActor* Caster = GetOwner() ? GetOwner() : this;
    bool bWasCritical = false;

    // 1. 데미지 계산
    float RawDamage = 0.f;
    if (Damage > 0.f)
    {
        RawDamage = UNonDamageHelpers::ComputeDamageFromAttributes(
            Caster,
            Damage, 
            DamageStatType,
            &bWasCritical
        );
    }

    // 2. 방어력 적용
    float FinalDamage = 0.f;
    if (RawDamage > 0.f)
    {
        FinalDamage = UNonDamageHelpers::ApplyDefenseReduction(Other, RawDamage, DamageStatType);
    }

    // 데미지도 0 이하이고 피격 태그도 없고 추가 효과도 없으면 무시
    if (FinalDamage <= 0.f && !HitReactionTag.IsValid() && !AdditionalEffect) return;

    // 3. 실제 적용
    if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(Other))
    {
        Player->ApplyDamageAt(FinalDamage, Caster, HitPoint, HitReactionTag);
    }
    else if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Other))
    {
        Enemy->ApplyDamageAt(FinalDamage, Caster, HitPoint, bWasCritical, HitReactionTag);
    }
    else if (FinalDamage > 0.f)
    {
        const FVector Dir = (HitPoint - GetActorLocation()).GetSafeNormal();
        UGameplayStatics::ApplyPointDamage(
             Other, FinalDamage, Dir, FHitResult(), 
             Caster->GetInstigatorController(), Caster, UDamageType::StaticClass()
        );
    }

    // --- [New] 상태이상(GE) 적용 (서버에서만 실행) ---
    if (AdditionalEffect && Other->HasAuthority())
    {
        if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Other))
        {
            FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false);
            if (!DeadTag.IsValid() || !ASC->HasMatchingGameplayTag(DeadTag))
            {
                FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
                Ctx.AddInstigator(Caster, Caster ? Caster->GetInstigatorController() : nullptr);

            float EffectLevel = 1.0f;
            float StunDuration = 0.0f;
            if (ANonCharacterBase* AttackerChar = Cast<ANonCharacterBase>(Caster)) {
                EffectLevel = static_cast<float>(AttackerChar->GetLastSkillLevel());
                StunDuration = AttackerChar->GetLastSkillStunDuration();
            }

            FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(AdditionalEffect, EffectLevel, Ctx);
            
            UE_LOG(LogTemp, Warning, TEXT("[StunDebug] DamageAOE Caster: %s | Target: %s | StunDuration: %f | AdditionalEffect: %s"),
                Caster ? *Caster->GetName() : TEXT("Null"), *Other->GetName(), StunDuration, AdditionalEffect ? *AdditionalEffect->GetName() : TEXT("Null"));

            if (SpecHandle.IsValid())
            {
                if (StunDuration > 0.001f)
                {
                    SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Data.StunDuration")), StunDuration);
                    SpecHandle.Data->SetSetByCallerMagnitude(FName(TEXT("Data.StunDuration")), StunDuration);
                    SpecHandle.Data->Duration = StunDuration; // C++ 강제 지속시간 오버라이드
                }
                FActiveGameplayEffectHandle ActiveHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
                UE_LOG(LogTemp, Warning, TEXT("[StunDebug] DamageAOE Apply Result - IsValid: %s | Duration: %f"),
                    ActiveHandle.IsValid() ? TEXT("True") : TEXT("False"), SpecHandle.Data->GetDuration());
            }
        }
    }
}
    // -----------------------------------------------------
}

void ADamageAOE::DoHit()
{
    UWorld* World = GetWorld();
    if (!World) return;

    const FTransform Trans = ResolveTransform();
    const FVector Center = Trans.GetLocation();
    const FQuat Quat = Trans.GetRotation();

    TArray<AActor*> OverlappedActors;
    TArray<AActor*> IgnoreActors;
    IgnoreActors.Add(this);
    if (GetOwner()) IgnoreActors.Add(GetOwner());

    // 오브젝트 타입 설정 (Pawn 중심)
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    // 필요시 PhysicsBody, WorldDynamic 추가 가능

    bool bHit = false;

    // ── 형태별 판정 ──
    if (Shape == EAOEShape::Box)
    {
        bHit = UKismetSystemLibrary::BoxOverlapActors(
            World, Center, BoxExtent, ObjectTypes, AActor::StaticClass(), IgnoreActors, OverlappedActors
        );
        
        if (bDebugDraw)
        {
            DrawDebugBox(World, Center, BoxExtent, Quat, FColor::Red, false, DebugDrawTime);
        }
    }
    else if (Shape == EAOEShape::Sphere)
    {
        bHit = UKismetSystemLibrary::SphereOverlapActors(
            World, Center, Radius, ObjectTypes, AActor::StaticClass(), IgnoreActors, OverlappedActors
        );

        if (bDebugDraw)
        {
            DrawDebugSphere(World, Center, Radius, 16, FColor::Red, false, DebugDrawTime);
        }
    }
    else if (Shape == EAOEShape::Capsule)
    {
        bHit = UKismetSystemLibrary::CapsuleOverlapActors(
            World, Center, Radius, CapsuleHalfHeight, ObjectTypes, AActor::StaticClass(), IgnoreActors, OverlappedActors
        );

        if (bDebugDraw)
        {
            DrawDebugCapsule(World, Center, CapsuleHalfHeight, Radius, Quat, FColor::Red, false, DebugDrawTime);
        }
    }

    // ── 결과 처리 ──
    for (AActor* HitActor : OverlappedActors)
    {
        if (!IsValidTarget(HitActor)) continue;
        
        // 싱글 히트 옵션이고 이미 맞았다면 패스
        if (bSingleHitPerActor && HitActors.Contains(HitActor)) continue;

        ApplyDamageTo(HitActor, HitActor->GetActorLocation());

        if (bSingleHitPerActor)
        {
            HitActors.Add(HitActor);
        }
    }
}
