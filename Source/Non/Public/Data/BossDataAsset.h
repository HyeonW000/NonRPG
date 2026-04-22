#pragma once

#include "CoreMinimal.h"
#include "Data/EnemyDataAsset.h"
#include "GameplayAbilitySpec.h"
#include "BossDataAsset.generated.h"

class UGameplayAbility;
class UAnimMontage;

USTRUCT(BlueprintType)
struct FBossPhaseData
{
    GENERATED_BODY()

    // 이 페이즈가 유지되는 체력 하한선 (예: 1페이즈 값이 0.5면 50% 아래일 때 다음 페이즈 진입)
    // 마지막 페이즈의 경우 이 값은 무시됩니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
    float HealthThresholdPercent = 0.5f;

    // 이 페이즈로 넘어갈 때 사용할 전환 몽타주 (포효 등)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
    UAnimMontage* TransitionMontage;

    // 이 페이즈 돌입 시 무적 시간 (몽타주 길이에 맞추거나 직접 지정)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
    float InvincibilityDuration = 3.0f;

    // 사용 가능한 스킬 목록 (GAS)
    // AI는 이 목록에서 쿨타임과 거리를 판단하여 무작위로 활성화합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase|Skills")
    TArray<TSubclassOf<UGameplayAbility>> GrantedSkills;
};

UCLASS(BlueprintType)
class NON_API UBossDataAsset : public UEnemyDataAsset
{
    GENERATED_BODY()

public:
    // 보스의 각 페이즈별 정보.
    // 배열 인덱스 0 = Phase 1, 인덱스 1 = Phase 2...
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
    TArray<FBossPhaseData> PhaseList;
};
