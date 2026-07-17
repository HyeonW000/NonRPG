// NonProjectile.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/NonDamageHelpers.h"
#include "GameplayTagContainer.h"
#include "NonProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UGameplayEffect;

UCLASS()
class NON_API ANonProjectile : public AActor
{
    GENERATED_BODY()

public:
    ANonProjectile();

    /** 충돌 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
    TObjectPtr<USphereComponent> CollisionComponent;

    /** 비주얼 메쉬 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
    TObjectPtr<UStaticMeshComponent> ProjectileMesh;

    /** 무브먼트 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

    /** 발사 속도 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0"))
    float Speed = 2000.f;

    /** 최대 사거리 제한용 수명 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0"))
    float Lifespan = 5.f;

    // ── 유도(Homing) 설정 ──
    /** 유도탄(Homing) 활성화 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Homing")
    bool bIsHoming = false;

    /** 유도 강도 (방향 전환 가속도) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Homing", meta = (EditCondition = "bIsHoming"))
    float HomingAcceleration = 5000.f;

    /** 유도 모드일 때 락온된 타겟 이외의 다른 캐릭터(몬스터)를 관통하고 통과할지 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Homing", meta = (EditCondition = "bIsHoming"))
    bool bIgnoreNonTargets = false;

    // ── 데미지 및 효과 설정 ──
    /** 데미지 배율 (시전자 공격력의 배수) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Damage")
    float BaseDamage = 1.0f;

    /** 데미지 타입 (물리/마법) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Damage")
    ENonDamageType DamageType = ENonDamageType::Physical;

    /** 피격 리액션 태그 (넉백, 경직 등) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Damage")
    FGameplayTag HitReactionTag;

    /** 추가 상태이상 마법 효과 (화상, 스턴 등) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Damage")
    TSubclassOf<UGameplayEffect> AdditionalEffect;

    /** 100% 필중(Perfect Tracking) 여부. 물리 가속도 오버슛 없이 대상을 완벽하게 추적하여 명중시킵니다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile|Homing", meta = (EditCondition = "bIsHoming"))
    bool bPerfectTracking = false;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION()
    void OnProjectileOverlap(
        UPrimitiveComponent* OverlappedComponent, 
        AActor* OtherActor, 
        UPrimitiveComponent* OtherComp, 
        int32 OtherBodyIndex, 
        bool bFromSweep, 
        const FHitResult& SweepResult);

private:
    UPROPERTY()
    TWeakObjectPtr<AActor> CachedTargetActor = nullptr;

    FVector CachedTargetLoc = FVector::ZeroVector;

    bool IsValidTarget(AActor* Other) const;
    void ApplyDamageTo(AActor* Other, const FVector& HitPoint);
};
