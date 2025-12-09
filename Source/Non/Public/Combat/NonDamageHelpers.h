#pragma once

#include "CoreMinimal.h"
#include "NonDamageHelpers.generated.h"

UENUM(BlueprintType)
enum class ENonDamageType : uint8
{
    Physical UMETA(DisplayName = "Physical"),
    Magical  UMETA(DisplayName = "Magical"),
};

UCLASS()
class NON_API UNonDamageHelpers : public UObject
{
    GENERATED_BODY()

public:
    // AttributeSet에서 값 읽어서 최종 데미지 계산
    // - SourceActor : 공격자 (캐릭터, 투사체의 Owner 등)
    // - PowerScale  : 스킬 계수 (평타면 노티파이 Damage, 스킬이면 Ability에서 정한 계수)
    // - DamageType  : 물리/마법 선택
    static float ComputeDamageFromAttributes(
        AActor* SourceActor,
        float PowerScale,
        ENonDamageType DamageType,
        bool* bOutWasCritical = nullptr);

    //  피격자의 방어/마저 적용
    UFUNCTION(BlueprintCallable, Category = "Non|Damage")
    static float ApplyDefenseReduction(
        AActor* TargetActor,
        float RawDamage,
        ENonDamageType DamageType);
};
