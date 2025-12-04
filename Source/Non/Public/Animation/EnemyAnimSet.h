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

    // 히트 리액션(코드에서 재생)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
    TObjectPtr<UAnimMontage> HitReact_F = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
    TObjectPtr<UAnimMontage> HitReact_B = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
    TObjectPtr<UAnimMontage> HitReact_L = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
    TObjectPtr<UAnimMontage> HitReact_R = nullptr;

    // 공격 몽타주들(랜덤/섹션으로 운용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
    TArray<TObjectPtr<UAnimMontage>> AttackMontages;

    // 공격용 재생 슬롯 (없으면 몽타주 기본 슬롯 사용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
    FName AttackSlotName = NAME_None;

    // 사망 몽타주 옵션
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
    bool bUseDeathMontage = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (EditCondition = "bUseDeathMontage"))
    TObjectPtr<UAnimMontage> DeathMontage = nullptr;
};
