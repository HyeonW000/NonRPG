#include "Combat/DamageSphereAOE.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

#include "AI/EnemyCharacter.h"
#include "Character/NonCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"

ADamageSphereAOE::ADamageSphereAOE()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true; // 위치 동기화 정도만 필요
    SetReplicateMovement(true);
}

void ADamageSphereAOE::Configure(float InRadius, float InDamage, float InDuration, float InInterval,
    bool bInSingleHit, ETeamSide InTeam, bool bInAttachToSocket,
    FName InSocket, FVector InOffset, bool bInServerOnly)
{
    Radius = InRadius;
    Damage = InDamage;
    Duration = InDuration;
    TickInterval = InInterval;
    bSingleHitPerActor = bInSingleHit;
    Team = InTeam;
    bAttachToOwnerSocket = bInAttachToSocket;
    AttachSocketName = InSocket;
    RelativeOffset = InOffset;
    bServerOnly = bInServerOnly;
}

void ADamageSphereAOE::BeginPlay()
{
    Super::BeginPlay();

    // 서버 전용 처리
    if (bServerOnly && !HasAuthority())
    {
        SetLifeSpan(Duration);
        return;
    }

    // 소켓 따라가기 (Owner의 Mesh/Scene에서 찾기)
    if (bAttachToOwnerSocket && GetOwner())
    {
        USceneComponent* Base = nullptr;

        // 1) Enemy/Player Mesh 우선
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

    // 즉시 1회, 이후 반복
    DoHit();
    if (TickInterval > 0.f)
    {
        GetWorldTimerManager().SetTimer(TickTimer, this, &ADamageSphereAOE::DoHit, TickInterval, true);
    }
    SetLifeSpan(Duration);
}

FVector ADamageSphereAOE::ResolveCenter() const
{
    FVector Center = GetActorLocation();
    if (FollowComp.IsValid())
    {
        if (AttachSocketName != NAME_None && FollowComp->DoesSocketExist(AttachSocketName))
        {
            Center = FollowComp->GetSocketLocation(AttachSocketName) + RelativeOffset;
        }
        else
        {
            Center = FollowComp->GetComponentLocation() + RelativeOffset;
        }
    }
    return Center;
}

bool ADamageSphereAOE::IsValidTarget(AActor* Other) const
{
    if (!Other || Other == this || Other == GetOwner()) return false;

    // 간단 팀 필터 (원하면 GameplayTags/Interface로 교체)
    switch (Team)
    {
    case ETeamSide::Enemy:
        // 적이 생성 → 타겟은 플레이어
        if (Cast<ANonCharacterBase>(Other)) return true;
        break;
    case ETeamSide::Player:
        // 플레이어가 생성 → 타겟은 적
        if (Cast<AEnemyCharacter>(Other)) return true;
        break;
    default:
        return true;
    }
    return false;
}

void ADamageSphereAOE::ApplyDamageTo(AActor* Other, const FVector& HitPoint)
{
    // 플레이어/적 공통: 각 클래스가 이미 ApplyDamageAt(Damage, Instigator, Point) 보유
    if (ANonCharacterBase* Player = Cast<ANonCharacterBase>(Other))
    {
        Player->ApplyDamageAt(Damage, GetOwner() ? GetOwner() : this, HitPoint);
        return;
    }
    if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Other))
    {
        Enemy->ApplyDamageAt(Damage, GetOwner() ? GetOwner() : this, HitPoint);
        return;
    }

    // 폴백: 일반 포인트 데미지
    UGameplayStatics::ApplyPointDamage(
        Other, Damage, FVector::UpVector, FHitResult(),
        GetInstigatorController(), GetOwner(), UDamageType::StaticClass());
}

void ADamageSphereAOE::DoHit()
{
    UWorld* World = GetWorld();
    if (!World) return;

    const FVector Center = ResolveCenter();

    // Overlap으로 가볍게! (Pawn/ObjectType 필터 필요하면 세팅)
    TArray<AActor*> Overlapped;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjTypes;
    ObjTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn)); // Pawn 중심

    TArray<AActor*> Ignore;
    if (AActor* O = GetOwner()) Ignore.Add(O);
    Ignore.Add(this);

    UKismetSystemLibrary::SphereOverlapActors(
        World, Center, Radius, ObjTypes, AActor::StaticClass(), Ignore, Overlapped
    );

    // 디버그
    if (bDebugDraw)
    {
        DrawDebugSphere(World, Center, Radius, 20, FColor::Red, false, DebugDrawTime, 0, 2.f);
    }

    for (AActor* A : Overlapped)
    {
        if (!IsValidTarget(A)) continue;
        if (bSingleHitPerActor && HitActors.Contains(A)) continue;

        ApplyDamageTo(A, A->GetActorLocation());

        if (bSingleHitPerActor) HitActors.Add(A);
    }
}
