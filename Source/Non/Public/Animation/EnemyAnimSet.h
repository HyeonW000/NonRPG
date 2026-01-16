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

    // 공격 몽타주들(랜덤/섹션으로 운용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
    TArray<TObjectPtr<UAnimMontage>> AttackMontages;
};
