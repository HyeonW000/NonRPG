#include "Combat/DamageAOE.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Components/SceneComponent.h"
#include "AI/EnemyCharacter.h"
#include "Character/NonCharacterBase.h"

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

void ADamageAOE::BeginPlay()
{
    Super::BeginPlay();

    if (bServerOnly && !HasAuthority())
    {
        SetLifeSpan(Duration);
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

    // 첫 타격 실행
    DoHit();

    // 반복 실행 설정
    if (TickInterval > 0.001f)
    {
        GetWorldTimerManager().SetTimer(TickTimer, this, &ADamageAOE::DoHit, TickInterval, true);
    }
    
    // 수명 설정
    SetLifeSpan(Duration);
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
    const float RawDamage = UNonDamageHelpers::ComputeDamageFromAttributes(
        Caster,
        Damage, 
        DamageStatType,
        &bWasCritical
    );

    if (RawDamage <= 0.f) return;

    // 2. 방어력 적용
    const float FinalDamage = UNonDamageHelpers::ApplyDefenseReduction(Other, RawDamage, DamageStatType);
    if (FinalDamage <= 0.f) return;

    // Debug
    // UE_LOG(LogTemp, Log, TEXT("[AOE] %s hit %s : Damage=%.1f"), *GetName(), *Other->GetName(), FinalDamage);

    // 3. 실제 적용
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
