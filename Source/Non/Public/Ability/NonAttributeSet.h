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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_HP, Category = "Attributes")
    FGameplayAttributeData HP;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, HP)

    // 들어오는 데미지 (Meta Attribute: 임시 저장용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FGameplayAttributeData IncomingDamage;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, IncomingDamage)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MaxHP, Category = "Attributes")
    FGameplayAttributeData MaxHP;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MaxHP)
    

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MP, Category = "Attributes")
    FGameplayAttributeData MP;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MP)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MaxMP, Category = "Attributes")
    FGameplayAttributeData MaxMP;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MaxMP)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_SP, Category = "Attributes")
    FGameplayAttributeData SP;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, SP)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MaxSP, Category = "Attributes")
    FGameplayAttributeData MaxSP;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MaxSP)

    // 물리 공격력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_AttackPower, Category = "Attributes")
    FGameplayAttributeData AttackPower;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, AttackPower)

    // 최소 물리 공격력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MinAttackPower, Category = "Attributes")
    FGameplayAttributeData MinAttackPower;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MinAttackPower)

    // 최대 물리 공격력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MaxAttackPower, Category = "Attributes")
    FGameplayAttributeData MaxAttackPower;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MaxAttackPower)
    
    // 마법 공격력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MagicPower, Category = "Attributes")
    FGameplayAttributeData MagicPower;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MagicPower)
    
    // 최소 마법 공격력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MinMagicPower, Category = "Attributes")
    FGameplayAttributeData MinMagicPower;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MinMagicPower)

    // 최대 마법 공격력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MaxMagicPower, Category = "Attributes")
    FGameplayAttributeData MaxMagicPower;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MaxMagicPower)

    // 크리티컬 확률
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_CriticalRate, Category = "Attributes")
    FGameplayAttributeData CriticalRate;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, CriticalRate)

    // 크리티컬 데미지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_CriticalDamage, Category = "Attributes")
    FGameplayAttributeData CriticalDamage;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, CriticalDamage)

    // 방어
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Defense, Category = "Attributes")
    FGameplayAttributeData Defense;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, Defense)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MagicResist, Category = "Attributes")
    FGameplayAttributeData MagicResist;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MagicResist)

    // 기타
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MoveSpeed, Category = "Attributes")
    FGameplayAttributeData MoveSpeed;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MoveSpeed)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_CooldownReduction, Category = "Attributes")
    FGameplayAttributeData CooldownReduction;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, CooldownReduction)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_HPRegen, Category = "Attributes")
    FGameplayAttributeData HPRegen;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, HPRegen)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_MPRegen, Category = "Attributes")
    FGameplayAttributeData MPRegen;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, MPRegen)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_SPRegen, Category = "Attributes")
    FGameplayAttributeData SPRegen;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, SPRegen)

    // 성장
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Level, Category = "Attributes")
    FGameplayAttributeData Level;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, Level)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Exp, Category = "Attributes")
    FGameplayAttributeData Exp;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, Exp)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_ExpToNextLevel, Category = "Attributes")
    FGameplayAttributeData ExpToNextLevel;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, ExpToNextLevel)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_StatPoint, Category = "Level")
    FGameplayAttributeData StatPoint;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, StatPoint)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_SkillPoint, Category = "Level")
    FGameplayAttributeData SkillPoint;
    ATTRIBUTE_ACCESSORS(UNonAttributeSet, SkillPoint)

protected:
    // AttackPower 변경 시 Min/Max 자동 재계산
    void RecalcAttackRangesFromBase();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION() virtual void OnRep_HP(const FGameplayAttributeData& OldHP);
    UFUNCTION() virtual void OnRep_MaxHP(const FGameplayAttributeData& OldMaxHP);
    UFUNCTION() virtual void OnRep_MP(const FGameplayAttributeData& OldMP);
    UFUNCTION() virtual void OnRep_MaxMP(const FGameplayAttributeData& OldMaxMP);
    UFUNCTION() virtual void OnRep_SP(const FGameplayAttributeData& OldSP);
    UFUNCTION() virtual void OnRep_MaxSP(const FGameplayAttributeData& OldMaxSP);

    UFUNCTION() virtual void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MinAttackPower(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MaxAttackPower(const FGameplayAttributeData& OldValue);
    
    UFUNCTION() virtual void OnRep_MagicPower(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MinMagicPower(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MaxMagicPower(const FGameplayAttributeData& OldValue);
    
    UFUNCTION() virtual void OnRep_CriticalRate(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_CriticalDamage(const FGameplayAttributeData& OldValue);
    
    UFUNCTION() virtual void OnRep_Defense(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MagicResist(const FGameplayAttributeData& OldValue);
    
    UFUNCTION() virtual void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_CooldownReduction(const FGameplayAttributeData& OldValue);
    
    UFUNCTION() virtual void OnRep_HPRegen(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MPRegen(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_SPRegen(const FGameplayAttributeData& OldValue);

    UFUNCTION() virtual void OnRep_Level(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_Exp(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ExpToNextLevel(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_StatPoint(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_SkillPoint(const FGameplayAttributeData& OldValue);

    // MagicPower 변경 시 Min/Max 자동 재계산
    void RecalcMagicRangesFromBase();
};
