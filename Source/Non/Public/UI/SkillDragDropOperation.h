#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Skill/SkillTypes.h"      // FSkillRow.Icon 타입용 (TSoftObjectPtr<UTexture2D>)
#include "SkillDragDropOperation.generated.h"

UCLASS()
class NON_API USkillDragDropOperation : public UDragDropOperation
{
    GENERATED_BODY()
public:
    // 어떤 스킬인지
    UPROPERTY(BlueprintReadOnly)
    FName SkillId = NAME_None;

    // 아이콘(소프트 레퍼런스 그대로 들고감)
    UPROPERTY(BlueprintReadOnly)
    TSoftObjectPtr<UTexture2D> Icon;
};
