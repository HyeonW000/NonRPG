#include "Combat/CombatSlotComponent.h"
#include "Character/EnemyCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UCombatSlotComponent::UCombatSlotComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UCombatSlotComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeSlots();
}

void UCombatSlotComponent::InitializeSlots()
{
    Slots.Empty();
    if (NumSlots <= 0)
    {
        NumSlots = 8;
    }

    const float AngleStep = 360.0f / static_cast<float>(NumSlots);
    for (int32 i = 0; i < NumSlots; ++i)
    {
        FCombatSlot NewSlot;
        NewSlot.SlotIndex = i;
        NewSlot.AngleOffsetDeg = static_cast<float>(i) * AngleStep;
        NewSlot.ClaimedEnemy = nullptr;
        Slots.Add(NewSlot);
    }
}

int32 UCombatSlotComponent::ReserveSlot(AEnemyCharacter* Enemy)
{
    if (!Enemy) return -1;

    if (Slots.Num() != NumSlots)
    {
        InitializeSlots();
    }

    // 1) 이미 해당 몬스터가 점유 중인 슬롯이 있다면 해당 슬롯 유지
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (Slots[i].ClaimedEnemy.Get() == Enemy)
        {
            return i;
        }
    }

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return -1;

    // 2) 몬스터의 현재 접근 각도 계산 (플레이어 정면 기준 상대 각도)
    const FVector OwnerLoc = OwnerActor->GetActorLocation();
    const FVector EnemyLoc = Enemy->GetActorLocation();
    const FVector DirToEnemy = (EnemyLoc - OwnerLoc).GetSafeNormal2D();
    const FVector OwnerForward = OwnerActor->GetActorForwardVector().GetSafeNormal2D();

    float TargetAngleDeg = 0.0f;
    if (!DirToEnemy.IsNearlyZero() && !OwnerForward.IsNearlyZero())
    {
        const float Dot = FVector::DotProduct(OwnerForward, DirToEnemy);
        TargetAngleDeg = FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)) * (180.0f / UE_PI);
        const FVector Cross = FVector::CrossProduct(OwnerForward, DirToEnemy);
        if (Cross.Z < 0.0f)
        {
            TargetAngleDeg = 360.0f - TargetAngleDeg;
        }
    }

    // 3) 비어있는 슬롯 중 대상 각도와 가장 차이가 적은 슬롯 선택
    int32 BestEmptyIndex = -1;
    float MinAngleDiff = 360.0f;

    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        // 유효하지 않거나 사망한 몬스터는 자동 정리
        if (Slots[i].ClaimedEnemy.IsValid())
        {
            AEnemyCharacter* Occupant = Slots[i].ClaimedEnemy.Get();
            if (!Occupant || Occupant->IsDead())
            {
                Slots[i].ClaimedEnemy = nullptr;
            }
        }

        if (!Slots[i].IsClaimed())
        {
            float Diff = FMath::Abs(Slots[i].AngleOffsetDeg - TargetAngleDeg);
            if (Diff > 180.0f)
            {
                Diff = 360.0f - Diff;
            }

            if (Diff < MinAngleDiff)
            {
                MinAngleDiff = Diff;
                BestEmptyIndex = i;
            }
        }
    }

    if (BestEmptyIndex != -1)
    {
        Slots[BestEmptyIndex].ClaimedEnemy = Enemy;
        return BestEmptyIndex;
    }

    // 4) 만약 모든 슬롯이 꽉 찼다면, 가장 가까운 슬롯을 임시 공유 반환
    int32 NearestIndex = 0;
    float MinNearestDiff = 360.0f;
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        float Diff = FMath::Abs(Slots[i].AngleOffsetDeg - TargetAngleDeg);
        if (Diff > 180.0f)
        {
            Diff = 360.0f - Diff;
        }
        if (Diff < MinNearestDiff)
        {
            MinNearestDiff = Diff;
            NearestIndex = i;
        }
    }

    return NearestIndex;
}

void UCombatSlotComponent::ReleaseSlot(AEnemyCharacter* Enemy)
{
    if (!Enemy) return;

    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (Slots[i].ClaimedEnemy.Get() == Enemy)
        {
            Slots[i].ClaimedEnemy = nullptr;
        }
    }
}

void UCombatSlotComponent::ReleaseAllSlots()
{
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        Slots[i].ClaimedEnemy = nullptr;
    }
}

bool UCombatSlotComponent::HasSlot(const AEnemyCharacter* Enemy) const
{
    if (!Enemy) return false;

    for (const FCombatSlot& Slot : Slots)
    {
        if (Slot.ClaimedEnemy.Get() == Enemy)
        {
            return true;
        }
    }
    return false;
}

FVector UCombatSlotComponent::GetSlotLocation(const AEnemyCharacter* Enemy, float CustomRadius) const
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return FVector::ZeroVector;

    const FVector OwnerLoc = OwnerActor->GetActorLocation();

    if (!Enemy) return OwnerLoc;

    int32 FoundIndex = -1;
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (Slots[i].ClaimedEnemy.Get() == Enemy)
        {
            FoundIndex = i;
            break;
        }
    }

    if (FoundIndex != -1)
    {
        return GetSlotLocationByIndex(FoundIndex, CustomRadius);
    }

    // 아직 등록되지 않은 경우 Enemy 위치 방향으로 즉시 계산
    const float Radius = (CustomRadius > 0.0f) ? CustomRadius : DefaultSlotRadius;
    const FVector Dir = (Enemy->GetActorLocation() - OwnerLoc).GetSafeNormal2D();
    return OwnerLoc + (Dir * Radius);
}

FVector UCombatSlotComponent::GetSlotLocationByIndex(int32 SlotIndex, float CustomRadius) const
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return FVector::ZeroVector;

    const FVector OwnerLoc = OwnerActor->GetActorLocation();
    if (!Slots.IsValidIndex(SlotIndex))
    {
        return OwnerLoc;
    }

    const float Radius = (CustomRadius > 0.0f) ? CustomRadius : DefaultSlotRadius;
    const FRotator OwnerYawRot(0.f, OwnerActor->GetActorRotation().Yaw, 0.f);
    const FRotator SlotRot = OwnerYawRot + FRotator(0.f, Slots[SlotIndex].AngleOffsetDeg, 0.f);

    return OwnerLoc + (SlotRot.Vector() * Radius);
}

void UCombatSlotComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 에디터/인게임 디버그 시각화
    if (bDebugDraw && GetWorld())
    {
        AActor* OwnerActor = GetOwner();
        if (!OwnerActor) return;

        const FVector OwnerLoc = OwnerActor->GetActorLocation();

        for (int32 i = 0; i < Slots.Num(); ++i)
        {
            const FVector SlotPos = GetSlotLocationByIndex(i, DefaultSlotRadius);
            const bool bOccupied = Slots[i].IsClaimed();
            const FColor SlotColor = bOccupied ? FColor::Red : FColor::Green;

            DrawDebugSphere(GetWorld(), SlotPos, 20.0f, 8, SlotColor, false, -1.0f, 0, 1.5f);
            DrawDebugLine(GetWorld(), OwnerLoc, SlotPos, SlotColor, false, -1.0f, 0, 1.0f);

            if (bOccupied && Slots[i].ClaimedEnemy.IsValid())
            {
                const FString SlotText = FString::Printf(TEXT("Slot %d: %s"), i, *Slots[i].ClaimedEnemy->GetName());
                DrawDebugString(GetWorld(), SlotPos + FVector(0.f, 0.f, 30.f), SlotText, nullptr, FColor::Yellow, 0.0f, true);
            }
        }
    }
}
