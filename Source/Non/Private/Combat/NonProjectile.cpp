// NonProjectile.cpp
#include "Combat/NonProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Character/NonCharacterBase.h"
#include "Character/EnemyCharacter.h"
#include "Combat/DamageAOE.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ANonProjectile::ANonProjectile()
{
    PrimaryActorTick.bCanEverTick = true;

    // 충돌체 생성
    CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComponent->InitSphereRadius(15.f);
    CollisionComponent->SetCollisionProfileName(TEXT("Projectile"));
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionComponent->SetGenerateOverlapEvents(true);
    RootComponent = CollisionComponent;

    // 비주얼 메쉬 생성
    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 무브먼트 컴포넌트 설정
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = CollisionComponent;
    ProjectileMovement->InitialSpeed = Speed;
    ProjectileMovement->MaxSpeed = Speed;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = false;
    ProjectileMovement->ProjectileGravityScale = 0.f; // 중력 영향 없음
}

void ANonProjectile::BeginPlay()
{
    Super::BeginPlay();

    // 겹침 이벤트 바인딩
    CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ANonProjectile::OnProjectileOverlap);

    // 수명 설정
    SetLifeSpan(Lifespan);

    // 캐릭터의 최종 조준점으로 방향 정렬 및 유도 설정
    if (ANonCharacterBase* OwnerChar = Cast<ANonCharacterBase>(GetInstigator()))
    {
        CachedTargetLoc = OwnerChar->GetLastSkillTargetLocation();
        CachedTargetActor = OwnerChar->GetLastSkillTargetActor();

        AActor* TargetActor = CachedTargetActor.Get();

        // 1. 일반 유도 기능 활성화 및 유효 타겟 등록 (필중 모드가 아닐 때만 엔진 네이티브 유도 사용)
        if (bIsHoming && !bPerfectTracking && TargetActor && TargetActor->GetRootComponent())
        {
            ProjectileMovement->bIsHomingProjectile = true;
            ProjectileMovement->HomingAccelerationMagnitude = HomingAcceleration;
            ProjectileMovement->HomingTargetComponent = TargetActor->GetRootComponent();
        }

        // 2. 초기 각도 및 물리 발사 방향 초기화
        if (!CachedTargetLoc.IsZero())
        {
            FVector SpawnLoc = GetActorLocation();
            FVector Direction = (CachedTargetLoc - SpawnLoc).GetSafeNormal();
            
            // 방향 벡터로 회전값 산출 후 설정
            FRotator TargetRot = Direction.Rotation();
            SetActorRotation(TargetRot);

            // 무브먼트 컴포넌트 속도 벡터 초기화
            ProjectileMovement->Velocity = Direction * Speed;
        }
    }

    // 3. 필중(Perfect Tracking) 모드가 아니라면 프레임 틱을 꺼서 성능 최적화
    if (!bPerfectTracking)
    {
        PrimaryActorTick.bCanEverTick = false;
        SetActorTickEnabled(false);
    }
}

void ANonProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bPerfectTracking)
    {
        if (AActor* TargetActor = CachedTargetActor.Get())
        {
            FVector CurrentLoc = GetActorLocation();
            // 캐릭터의 가슴/중앙(Z축 +50)을 표적으로 잡음
            FVector TargetLoc = TargetActor->GetActorLocation() + FVector(0.f, 0.f, 50.f);

            FVector Direction = (TargetLoc - CurrentLoc).GetSafeNormal();

            // 프레임마다 오버슛 없이 방향을 즉시 타겟으로 리셋
            ProjectileMovement->Velocity = Direction * Speed;
            SetActorRotation(Direction.Rotation());

            // [터널링 현상 방지]
            // 프레임당 이동 거리보다 타겟과의 거리가 좁혀졌다면 물리 콜리전 겹침 판정을 건너뛰고 강제 명중 처리
            float Dist = FVector::Dist(CurrentLoc, TargetLoc);
            float TravelFrameDist = Speed * DeltaTime;

            if (Dist <= (TravelFrameDist + 80.f))
            {
                if (HasAuthority())
                {
                    ApplyDamageTo(TargetActor, TargetLoc);
                }
                Destroy();
            }
        }
    }
}

bool ANonProjectile::IsValidTarget(AActor* Other) const
{
    if (!Other || Other == this || Other == GetInstigator()) return false;

    // 팀 판정 (소환사가 EnemyCharacter면 Enemy, 아니면 Player)
    ETeamSideAOE FiredTeam = ETeamSideAOE::Player;
    if (GetInstigator() && GetInstigator()->IsA<AEnemyCharacter>())
    {
        FiredTeam = ETeamSideAOE::Enemy;
    }

    if (FiredTeam == ETeamSideAOE::Enemy)
    {
        // 적군이 쏜 것 -> 타겟은 플레이어
        if (Cast<ANonCharacterBase>(Other)) return true;
    }
    else if (FiredTeam == ETeamSideAOE::Player)
    {
        // 플레이어가 쏜 것 -> 타겟은 Enemy
        if (Cast<AEnemyCharacter>(Other)) return true;
    }

    return false;
}

void ANonProjectile::OnProjectileOverlap(
    UPrimitiveComponent* OverlappedComponent, 
    AActor* OtherActor, 
    UPrimitiveComponent* OtherComp, 
    int32 OtherBodyIndex, 
    bool bFromSweep, 
    const FHitResult& SweepResult)
{
    // 유효한 타겟이 아니면 통과
    if (!IsValidTarget(OtherActor))
    {
        return;
    }

    // [New] 유도 모드이고 타겟 관통 옵션이 켜져 있을 때의 관통 필터링
    if (bIsHoming && bIgnoreNonTargets)
    {
        USceneComponent* HomingComp = ProjectileMovement->HomingTargetComponent.Get();
        AActor* LockedTarget = HomingComp ? HomingComp->GetOwner() : nullptr;
        if (LockedTarget)
        {
            // 락온된 표적이 있다면, 그 대상이 아닌 다른 적과의 충돌은 무시하고 관통
            if (OtherActor != LockedTarget)
            {
                return;
            }
        }
        else
        {
            // 허공에 쐈다면(락온된 대상이 없다면), 날아가는 경로 상의 그 어떤 적 캐릭터(Pawn)와도 부딪치지 않고 관통
            if (OtherActor->IsA<APawn>())
            {
                return;
            }
        }
    }

    // 서버에서만 데미지 적용
    if (HasAuthority())
    {
        FVector ImpactLoc = bFromSweep ? FVector(SweepResult.ImpactPoint) : GetActorLocation();
        ApplyDamageTo(OtherActor, ImpactLoc);
    }

    // 투사체 소멸
    Destroy();
}

void ANonProjectile::ApplyDamageTo(AActor* Other, const FVector& HitPoint)
{
    if (!Other) return;

    AActor* Caster = GetInstigator();
    if (!Caster)
    {
        Caster = this;
    }
    bool bWasCritical = false;

    // 1. 캐릭터에서 스킬 계수 가져오기 (만약 BaseDamage가 오버라이드 안되었다면 캐릭터의 스킬 계수 사용)
    float FinalDamageScale = BaseDamage;
    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Caster))
    {
        FinalDamageScale = NonChar->GetLastSkillDamageScale();
    }

    // 2. 데미지 계산
    const float RawDamage = UNonDamageHelpers::ComputeDamageFromAttributes(
        Caster,
        FinalDamageScale, 
        DamageType,
        &bWasCritical
    );

    if (RawDamage <= 0.f) return;

    // 3. 방어력 적용
    const float FinalDamage = UNonDamageHelpers::ApplyDefenseReduction(Other, RawDamage, DamageType);
    if (FinalDamage <= 0.f) return;

    // 4. 실제 데미지 적용
    if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(Other))
    {
        Player->ApplyDamageAt(FinalDamage, Caster, HitPoint, HitReactionTag);
    }
    else if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Other))
    {
        Enemy->ApplyDamageAt(FinalDamage, Caster, HitPoint, bWasCritical, HitReactionTag);
    }
    else
    {
        const FVector Dir = (HitPoint - GetActorLocation()).GetSafeNormal();
        UGameplayStatics::ApplyPointDamage(
             Other, FinalDamage, Dir, FHitResult(), 
             Caster->GetInstigatorController(), Caster, UDamageType::StaticClass()
        );
    }

    // 5. 추가 상태이상(GameplayEffect) 적용 (서버에서만 실행)
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
            
            UE_LOG(LogTemp, Warning, TEXT("[StunDebug] Projectile Caster: %s | Target: %s | StunDuration: %f | AdditionalEffect: %s"),
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
                UE_LOG(LogTemp, Warning, TEXT("[StunDebug] Projectile Apply Result - IsValid: %s | Duration: %f"),
                    ActiveHandle.IsValid() ? TEXT("True") : TEXT("False"), SpecHandle.Data->GetDuration());
            }
        }
    }
}
}
