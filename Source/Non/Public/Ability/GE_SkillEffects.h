#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_SkillEffects.generated.h"

/**
 * 스킬 쿨타임/비용 공용 GameplayEffect (C++ 정의, 에셋 없음)
 * - 쿨타임: 지속 시간은 SetByCaller(SkillCooldown)로 매번 전달, 스킬별 쿨타임 태그는 Spec의 DynamicGrantedTags로 부여
 * - 비용: 차감량은 SetByCaller(SkillCost)로 매번 전달 (음수로 넣음)
 */
namespace NonSkillEffects
{
    // SetByCaller 이름 (태그가 아니라 이름을 써서 태그 등록 순서와 무관하게 동작)
    inline const FName CooldownDataName(TEXT("SkillCooldown"));
    inline const FName CostDataName(TEXT("SkillCost"));
}

/** 스킬 쿨타임 (HasDuration, 시간 = SetByCaller SkillCooldown) */
UCLASS()
class NON_API UGE_SkillCooldown : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UGE_SkillCooldown();
};

/** 스킬 비용 공통 (Instant, 대상 속성 += SetByCaller SkillCost) */
UCLASS(Abstract)
class NON_API UGE_SkillCostBase : public UGameplayEffect
{
    GENERATED_BODY()

protected:
    void InitCostModifier(const FGameplayAttribute& Attribute);
};

/** SP 소모 */
UCLASS()
class NON_API UGE_SkillCost_SP : public UGE_SkillCostBase
{
    GENERATED_BODY()

public:
    UGE_SkillCost_SP();
};

/** MP 소모 */
UCLASS()
class NON_API UGE_SkillCost_MP : public UGE_SkillCostBase
{
    GENERATED_BODY()

public:
    UGE_SkillCost_MP();
};

/** HP 소모 (HP 1 미만으로 내리지 않는 처리는 적용하는 쪽에서 차감량을 조절) */
UCLASS()
class NON_API UGE_SkillCost_HP : public UGE_SkillCostBase
{
    GENERATED_BODY()

public:
    UGE_SkillCost_HP();
};
