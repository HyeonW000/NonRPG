#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSetTypes.h"
#include "AnimSet_Common.generated.h"

/** 공통 애니메이션 세트(DataAsset) */
UCLASS(BlueprintType)
class NON_API UAnimSet_Common : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common")
    FCommonAnimSet Common;
};
