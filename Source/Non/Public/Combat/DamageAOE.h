#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/NonDamageHelpers.h"
#include "DamageAOE.generated.h"

class USceneComponent;

/** AOE 형태 정의 */
UENUM(BlueprintType)
enum class EAOEShape : uint8
{
    Box,
    Sphere,
    Capsule
};

/** 팀 구분 (통합됨) */
UENUM(BlueprintType)
enum class ETeamSideAOE : uint8
{
    Neutral,
    Player,
    Enemy
};

/** 
 * 통합된 광역 공격(AOE) 액터
 * Box, Sphere, Capsule 등 형태를 선택하여 사용 가능
 */
UCLASS()
class NON_API ADamageAOE : public AActor
{
    GENERATED_BODY()
public:
    ADamageAOE();

    // ── 공통 파라미터 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE|Shape")
    EAOEShape Shape = EAOEShape::Box;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE|Shape", meta = (EditCondition = "Shape==EAOEShape::Box", EditConditionHides))
    FVector BoxExtent = FVector(100.f, 100.f, 100.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE|Shape", meta = (EditCondition = "Shape==EAOEShape::Sphere || Shape==EAOEShape::Capsule", EditConditionHides))
    float Radius = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE|Shape", meta = (EditCondition = "Shape==EAOEShape::Capsule", EditConditionHides))
    float CapsuleHalfHeight = 200.f;

    // 데미지 (GAS 연동용 계수)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE|Damage")
    float Damage = 1.f;

    // 어떤 스탯 기반인지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE|Damage")
    ENonDamageType DamageStatType = ENonDamageType::Magical;

    // 생존 시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE|Life")
    float Duration = 0.5f;

    // 히트 체크 간격
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE|Life")
    float TickInterval = 0.1f;

    // 단일 대상 중복 히트 방지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE|Rule")
    bool bSingleHitPerActor = true;

    // 서버 전용 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE|Rule")
    bool bServerOnly = true;

    // 팀 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE|Rule")
    ETeamSideAOE Team = ETeamSideAOE::Enemy;
    
    // ── 어태치먼트 설정 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach")
    bool bAttachToOwnerSocket = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach", meta = (EditCondition = "bAttachToOwnerSocket"))
    FName AttachSocketName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach", meta = (EditCondition = "bAttachToOwnerSocket"))
    FVector RelativeOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach", meta = (EditCondition = "bAttachToOwnerSocket"))
    FRotator RelativeRotationOffset = FRotator::ZeroRotator;


    // ── 디버그 ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bDebugDraw = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    float DebugDrawTime = 0.1f;


    // ── 유틸리티 ──
    UFUNCTION(BlueprintCallable, Category = "AOE|Damage")
    void SetPowerScale(float InScale) { Damage = InScale; }

    /** 편의성 설정 함수 (BP/C++ 양용) */
    UFUNCTION(BlueprintCallable, Category = "AOE")
    void ConfigureBox(FVector InExtent, float InDamage, float InDuration, float InInterval = 0.1f, bool bSingleHit = true);

    UFUNCTION(BlueprintCallable, Category = "AOE")
    void ConfigureSphere(float InRadius, float InDamage, float InDuration, float InInterval = 0.1f, bool bSingleHit = true);

protected:
    virtual void BeginPlay() override;

private:
    FTimerHandle TickTimer;
    TSet<TWeakObjectPtr<AActor>> HitActors;
    TWeakObjectPtr<USceneComponent> FollowComp;

    void DoHit();
    FTransform ResolveTransform() const;
    bool IsValidTarget(AActor* Other) const;
    void ApplyDamageTo(AActor* Other, const FVector& HitPoint);
};
