#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "NonAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class NON_API UNonAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UNonAttributeSet();

    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

    // 생존
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData HP;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, HP)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData MaxHP;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MaxHP)
    

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData MP;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MP)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData MaxMP;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MaxMP)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData SP;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, SP)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData MaxSP;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MaxSP)

    // 공격
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData AttackPower;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, AttackPower)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData MagicPower;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MagicPower)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData CriticalRate;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, CriticalRate)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData CriticalDamage;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, CriticalDamage)

    // 방어
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData Defense;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, Defense)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData MagicResist;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MagicResist)

    // 기타
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData MoveSpeed;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MoveSpeed)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData CooldownReduction;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, CooldownReduction)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData HPRegen;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, HPRegen)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData MPRegen;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MPRegen)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData SPRegen;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, SPRegen)

    // 성장
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData Level;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, Level)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData Exp;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, Exp)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData ExpToNextLevel;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, ExpToNextLevel)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
    FGameplayAttributeData StatPoint;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, StatPoint)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
    FGameplayAttributeData SkillPoint;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, SkillPoint)
   

};
