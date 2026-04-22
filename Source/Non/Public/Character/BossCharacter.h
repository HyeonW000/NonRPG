#pragma once

#include "CoreMinimal.h"
#include "Character/EnemyCharacter.h"
#include "GameplayAbilitySpecHandle.h"
#include "BossCharacter.generated.h"

class USphereComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossPhaseChanged, int32, NewPhase);

class UBossDataAsset;
class UGameplayAbility;

UCLASS()
class NON_API ABossCharacter : public AEnemyCharacter
{
    GENERATED_BODY()

public:
    ABossCharacter();

    UPROPERTY(BlueprintAssignable, Category = "Boss|Events")
    FOnBossPhaseChanged OnBossPhaseChanged;

protected:
    virtual void BeginPlay() override;
    virtual void UpdateHPBar() const override;

public:
    // 데미지 무시(무적) 처리를 위한 오버라이드
    virtual void ApplyDamageAt(float Amount, AActor* DamageInstigator, const FVector& WorldLocation, bool bIsCritical = false, FGameplayTag ReactionTag = FGameplayTag()) override;

    // 데미지 숫자 위치 스폰 보정용 오버라이드
    virtual void Multicast_SpawnDamageNumber_Implementation(float Amount, FVector WorldLocation, bool bIsCritical) override;

    // 데이터에셋 정보 초기화 오버라이드 (BossDataAsset 사용)
    UFUNCTION(BlueprintCallable, Category = "Boss|Config")
    void InitBossData(const UBossDataAsset* InBossData);
    
public:
    // 보스 인지 반경 (거리에 가까워지면 보스 UI가 나타남)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|UI")
    USphereComponent* UIActivationZone;

    // DataAsset (블루프린트에서 직접 넣을 수 있도록 EditAnywhere 적용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Data")
    const UBossDataAsset* BossData;

    // 현재 페이즈 (1부터 시작)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Phase")
    int32 CurrentPhase;

    // 페이즈 전환 중인지 여부 (이때 무적)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Phase")
    bool bIsTransitioningPhase;

    // 현재 페이즈에서 부여받은 스킬들의 핸들 목록 (페이즈 넘어갈 때 해제하기 위함)
    TArray<FGameplayAbilitySpecHandle> PhaseAbilityHandles;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Boss|Phase")
    void ApplyPhase(int32 TargetPhase);

    // 무적 해제 콜백
    UFUNCTION()
    void EndPhaseTransition();

protected:
    UFUNCTION()
    virtual void OnUIZoneOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                                      bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    virtual void OnUIZoneOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    void CheckPhaseTransition();
};
