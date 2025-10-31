#pragma once

#include "Engine/DataAsset.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSetTypes.h"
#include "AnimSet_Common.generated.h"

UCLASS(BlueprintType)
class NON_API UAnimSet_Common : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Common")
    FClassCommonSet Common;
};
