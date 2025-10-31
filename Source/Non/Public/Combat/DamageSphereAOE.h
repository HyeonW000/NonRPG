#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DamageSphereAOE.generated.h"

// 간단 팀 구분(원하면 GameplayTags/Interface로 대체)
UENUM(BlueprintType)
enum class ETeamSide : uint8
{
    Neutral,
    Player,
    Enemy
};

UCLASS()
class NON_API ADamageSphereAOE : public AActor
{
    GENERATED_BODY()
public:
    ADamageSphereAOE();

    // ── 파라미터 (BP에서 설정하거나 스폰시 넘김) ──
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE")
    float Radius = 180.f;

    // GAS SetByCaller(Data.Damage) 로 들어갈 값(양수면 가한다; 내부에서 음수로 넣음)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE")
    float Damage = 15.f;

    // 생존 시간 (이 시간 동안 interval로 반복히트)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE")
    float Duration = 0.6f;

    // 히트 체크 간격(짧을수록 연속타격에 가깝고 비용은 ↑)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE")
    float TickInterval = 0.12f;

    // 한 액터당 1회만 히트(중복 방지)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE")
    bool bSingleHitPerActor = true;

    // 서버에서만 판정 수행
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOE")
    bool bServerOnly = true;

    // 소켓/오프셋 따라가기
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach")
    bool bAttachToOwnerSocket = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach", meta = (EditCondition = "bAttachToOwnerSocket"))
    FName AttachSocketName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach", meta = (EditCondition = "bAttachToOwnerSocket"))
    FVector RelativeOffset = FVector::ZeroVector;

    // 친/적 판정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
    ETeamSide Team = ETeamSide::Enemy;

    // 디버그
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bDebugDraw = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    float DebugDrawTime = 0.1f;

    // 스폰 후 한 번에 세팅하고 싶을 때(편의 정적 함수)
    UFUNCTION(BlueprintCallable, Category = "AOE")
    void Configure(float InRadius, float InDamage, float InDuration, float InInterval,
        bool bInSingleHit, ETeamSide InTeam, bool bInAttachToSocket = false,
        FName InSocket = NAME_None, FVector InOffset = FVector::ZeroVector, bool bInServerOnly = true);

protected:
    virtual void BeginPlay() override;

private:
    FTimerHandle TickTimer;
    TSet<TWeakObjectPtr<AActor>> HitActors;
    TWeakObjectPtr<USceneComponent> FollowComp;

    void DoHit();
    FVector ResolveCenter() const;
    bool IsValidTarget(AActor* Other) const;
    void ApplyDamageTo(AActor* Other, const FVector& HitPoint);
};
