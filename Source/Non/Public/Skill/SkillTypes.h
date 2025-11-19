#pragma once
#include "CoreMinimal.h"
#include "SkillTypes.generated.h"


class UGameplayAbility;
class UGameplayEffect;
class UAnimMontage;

UENUM(BlueprintType)
enum class EJobClass : uint8
{
    Defender, Berserker, Cleric
};

UENUM(BlueprintType)
enum class ESkillType : uint8
{
    Active, Passive
};

USTRUCT(BlueprintType)
struct FSkillRow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText  Name;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ESkillType Type = ESkillType::Active;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EJobClass  AllowedClass = EJobClass::Defender;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  MaxLevel = 3;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  RequiredCharacterLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    // 에디터에서 드롭해둘 아이콘(소프트 레퍼런스 권장)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Icon;

    // 액티브: 공용 GA (예: GA_Melee_Generic)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<UGameplayAbility> AbilityClass;

    // 애니/수치 파라미터(선택)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimMontage> Montage; // 필요 시
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<float> LevelScalars;       // 레벨별 계수(데미지 등)

    // 패시브: 공용 GE 템플릿(무한 지속, SetByCaller 또는 스택)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<UGameplayEffect> PassiveEffect;

    // SetByCaller 키(패시브/액티브 공통으로 쓰고 싶으면)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SetByCallerKey = "Data.SkillValue";
};



UCLASS(BlueprintType)
class USkillDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<FName, FSkillRow> Skills; // Id → 정의
};
