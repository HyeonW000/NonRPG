#include "Combat/DamageBoxAOE.h"
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

ADamageBoxAOE::ADamageBoxAOE()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true; // 위치 동기화 정도만 필요
    SetReplicateMovement(true);
}

void ADamageBoxAOE::Configure(FVector InBoxExtent, float InDamage, float InDuration, float InInterval,
    bool bInSingleHit, ETeamSideBox InTeam, bool bInAttachToSocket,
    FName InSocket, FVector InOffset, FRotator InRotOffset, bool bInServerOnly)
{
    BoxExtent = InBoxExtent;
    Damage = InDamage;
    Duration = InDuration;
    TickInterval = InInterval;
    bSingleHitPerActor = bInSingleHit;
    Team = InTeam;
    bAttachToOwnerSocket = bInAttachToSocket;
    AttachSocketName = InSocket;
    RelativeOffset = InOffset;
    RelativeRotationOffset = InRotOffset;
    bServerOnly = bInServerOnly;
}

void ADamageBoxAOE::BeginPlay()
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
        GetWorldTimerManager().SetTimer(TickTimer, this, &ADamageBoxAOE::DoHit, TickInterval, true);
    }
    SetLifeSpan(Duration);
}

FTransform ADamageBoxAOE::ResolveTransform() const
{
    FTransform Trans = GetActorTransform();
    
    // Attach 되어있으면 이미 따라가고 있겠지만, 
    // Offset/RotOffset을 추가로 적용하고 싶다면 여기서 계산
    // 현재는 AttachToComponent로 붙였으므로 RelativeOffset/Rotation은 
    // 컴포넌트의 RelativeTransform으로 설정하는게 맞을 수도 있으나,
    // 여기서는 단순하게 월드 기준으로 계산하거나, Attach 후 LocalOffset을 주는 방식 등을 고려.
    
    // 하지만 위 Configure/BeginPlay 구조상 AttachToComponent를 하면 
    // Actor의 Transform이 부모 소켓을 따라가게 됨.
    // 추가 Offset은 Actor의 RelativeLocation/Rotation으로 설정해주는 게 가장 깔끔함.
    
    // 만약 Attach를 안 했다면?
    // 그냥 현재 Actor Transform 사용.
    
    // 여기서는 "판정 중심"을 리턴하는 용도.
    // BoxOverlapActors는 Center, Rotation(Quat)를 받음.
    
    // 만약 Attach 상태라면 ActorLocation/Rotation이 이미 소켓+Relative 반영됨(SetRelative...를 했다면).
    // 하지만 위 코드엔 SetRelativeLocation 등을 하는 부분이 없음.
    // 따라서 여기서 직접 계산해서 리턴하거나, BeginPlay에서 SetRelative...를 해줘야 함.
    
    // 간단하게: 현재 Actor의 Transform을 그대로 쓴다.
    // 단, BeginPlay에서 Attach 후 RelativeOffset을 적용하는 로직이 빠져있었으므로
    // 여기서 보정하거나, BeginPlay를 수정하는게 좋음.
    
    // 수정: ResolveCenter() 처럼 직접 계산해서 리턴.
    
    FVector Center = GetActorLocation();
    FQuat Rotation = GetActorQuat();

    if (FollowComp.IsValid())
    {
        FTransform SocketTrans;
        if (AttachSocketName != NAME_None && FollowComp->DoesSocketExist(AttachSocketName))
        {
            SocketTrans = FollowComp->GetSocketTransform(AttachSocketName);
        }
        else
        {
            SocketTrans = FollowComp->GetComponentTransform();
        }
        
        // 소켓 트랜스폼에 오프셋 적용
        // (Socket * Offset)
        FTransform OffsetTrans(RelativeRotationOffset, RelativeOffset);
        FTransform FinalTrans = OffsetTrans * SocketTrans;
        
        Center = FinalTrans.GetLocation();
        Rotation = FinalTrans.GetRotation();
    }
    else
    {
        // Attach 안함 -> 그냥 현재 위치에 오프셋만 로컬로 적용? 
        // 아니면 그냥 현재 위치가 중심?
        // 보통 스폰할 때 위치를 잡고 스폰하므로, 추가 오프셋은 로컬 기준 적용
        FTransform ActorTrans = GetActorTransform();
        FTransform OffsetTrans(RelativeRotationOffset, RelativeOffset);
        FTransform FinalTrans = OffsetTrans * ActorTrans;
        
        Center = FinalTrans.GetLocation();
        Rotation = FinalTrans.GetRotation();
    }

    return FTransform(Rotation, Center);
}

bool ADamageBoxAOE::IsValidTarget(AActor* Other) const
{
    if (!Other || Other == this || Other == GetOwner()) return false;

    // 간단 팀 필터
    switch (Team)
    {
    case ETeamSideBox::Enemy:
        // 적이 생성 → 타겟은 플레이어
        if (Cast<ANonCharacterBase>(Other)) return true;
        break;
    case ETeamSideBox::Player:
        // 플레이어가 생성 → 타겟은 적
        if (Cast<AEnemyCharacter>(Other)) return true;
        break;
    default:
        return true;
    }
    return false;
}

void ADamageBoxAOE::ApplyDamageTo(AActor* Other, const FVector& HitPoint)
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
        Other, Damage, (HitPoint - GetActorLocation()).GetSafeNormal(), FHitResult(),
        GetInstigatorController(), GetOwner(), UDamageType::StaticClass());
}

void ADamageBoxAOE::DoHit()
{
    UWorld* World = GetWorld();
    if (!World) return;

    const FTransform Trans = ResolveTransform();
    const FVector Center = Trans.GetLocation();
    const FQuat Rotation = Trans.GetRotation();

    // Overlap
    TArray<AActor*> Overlapped;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjTypes;
    ObjTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn)); // Pawn 중심

    TArray<AActor*> Ignore;
    if (AActor* O = GetOwner()) Ignore.Add(O);
    Ignore.Add(this);

    UKismetSystemLibrary::BoxOverlapActors(
        World, Center, BoxExtent, ObjTypes, AActor::StaticClass(), Ignore, Overlapped
    );

    // 디버그
    if (bDebugDraw)
    {
        DrawDebugBox(World, Center, BoxExtent, Rotation, FColor::Red, false, DebugDrawTime, 0, 2.f);
    }

    for (AActor* A : Overlapped)
    {
        if (!IsValidTarget(A)) continue;
        if (bSingleHitPerActor && HitActors.Contains(A)) continue;

        ApplyDamageTo(A, A->GetActorLocation());

        if (bSingleHitPerActor) HitActors.Add(A);
    }
}
