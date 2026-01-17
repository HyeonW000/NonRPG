#pragma once

#include "Engine/DataAsset.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimMontage.h"
#include "EnemyAnimSet.generated.h"

class UAnimSequenceBase;
class UBlendSpace;
class UAnimMontage;

UCLASS(BlueprintType)
class NON_API UEnemyAnimSet : public UDataAsset
{
    GENERATED_BODY()
public:
    // 로코모션
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
    TObjectPtr<UBlendSpace> Locomotion = nullptr;

    // [Legacy Removed] HitReact_*, KnockdownMontage, DeathMontage
    // GAS (GA_HitReaction, GA_Death) 로 이관됨.
    // 각 GA Blueprint에서 몽타주를 설정하세요.

    // [Legacy Attack Removed]
    // GAS (GA_EnemyAttack) 로 이관됨.
    // GA_EnemyAttack Blueprint에서 몽타주를 설정하세요.
};
