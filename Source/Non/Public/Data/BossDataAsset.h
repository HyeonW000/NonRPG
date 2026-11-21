#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "BossDataAsset.generated.h"

class UGameplayAbility;
class UAnimMontage;

/**
 * 🌟 블루프린트 생성 없이 보스 데이터 에셋에서 바로 패턴을 입력하는 구조체
 */
USTRUCT(BlueprintType)
struct FBossSkillEntry
{
    GENERATED_BODY()

    // 스킬 식별 이름 (예: 내려찍기, 휘두르기, 바위던지기 등)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    FName SkillName = NAME_None;

    // 비헤이비어 트리와 연동되는 스킬 카테고리 태그
    // - 근접 공격: Enemy.Skill.Melee
    // - 원거리 공격: Enemy.Skill.RangeAttack
    // - 후방 공격: Enemy.Skill.BackAttack
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    FGameplayTag SkillCategoryTag;

    // 공격 애니메이션 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    TObjectPtr<UAnimMontage> Montage = nullptr;

    // 애니메이션 재생 속도 배율 (기본 1.0)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0.1"))
    float PlayRate = 1.0f;

    // 공격 사정거리 최소 거리 (cm 단위, 기본 0)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Range", meta = (ClampMin = "0.0"))
    float MinRange = 0.0f;

    // 공격 사정거리 최대 거리 (cm 단위, 기본 450)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Range", meta = (ClampMin = "0.0"))
    float MaxRange = 450.0f;

    // 개별 스킬 쿨타임 (초 단위, 기본 3.0초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cooldown", meta = (ClampMin = "0.0"))
    float Cooldown = 3.0f;

    // 스킬 선택 가중치/우선순위 (기본 1.0, 2.0이면 2배 더 자주 발동)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0.1"))
    float Weight = 1.0f;

    // 공격력 계수 (1.0 = 기본 100% 데미지, 1.5 = 150% 데미지)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0.0"))
    float DamageScale = 1.0f;
};

USTRUCT(BlueprintType)
struct FBossPhaseData
{
    GENERATED_BODY()

    // 이 페이즈가 유지되는 체력 하한선 (예: 1페이즈 값이 0.5면 50% 아래일 때 다음 페이즈 진입)
    // 마지막 페이즈의 경우 이 값은 무시됩니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
    float HealthThresholdPercent = 0.5f;

    // 이 페이즈로 넘어올 때 전환 연출(포효/변신 등)을 사용할지 여부 (1페이즈는 시작 상태이므로 꺼두시면 깔끔하게 숨겨집니다)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
    bool bUseTransitionMontage = false;

    // 이 페이즈로 넘어갈 때 사용할 전환 몽타주 (포효 등)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase", meta = (EditCondition = "bUseTransitionMontage", EditConditionHides))
    UAnimMontage* TransitionMontage = nullptr;

    // 🌟 [추천 방식] 블루프린트 생성 없이 몽타주와 쿨타임을 바로 등록하는 스킬 목록!
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase|Skills")
    TArray<FBossSkillEntry> Skills;

    // [기존 방식] 특수 기믹/유도 마법 등 복잡한 전용 GA 블루프린트가 필요할 때 사용하는 목록 (하이브리드 호환)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase|Skills")
    TArray<TSubclassOf<UGameplayAbility>> GrantedSkills;
};

UCLASS(BlueprintType)
class NON_API UBossDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    // 보스의 각 페이즈별 정보.
    // 배열 인덱스 0 = Phase 1, 인덱스 1 = Phase 2...
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
    TArray<FBossPhaseData> PhaseList;
};
