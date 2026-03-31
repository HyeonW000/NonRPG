#pragma once
#include "CoreMinimal.h"
#include "SkillTypes.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UAnimMontage;
class ADamageAOE;

// ─────────────────────────────────────────────────────────────────
// AOE 데이터 구조체 (에디터에서 Shape 다라지면 해당 옵션만 표시)
// ─────────────────────────────────────────────────────────────────

/** AOE 형태 (에디터 드롭다운 선택용) */
UENUM(BlueprintType)
enum class EAOEConfigShape : uint8
{
    Sphere   UMETA(DisplayName = "Sphere (구형)"),
    Box      UMETA(DisplayName = "Box (상자형)"),
    Capsule  UMETA(DisplayName = "Capsule (쳪슈지형)"),
};

USTRUCT(BlueprintType)
struct FAOEConfig
{
    GENERATED_BODY()

    /** 스폰할 DamageAOE 블루프린트 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AOE")
    TSubclassOf<ADamageAOE> AOEClass;

    /** 데칼(조준 표시) 블루프린트 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AOE")
    TSubclassOf<AActor> DecalClass;

    // ── 형태 선택 ──────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AOE|Shape")
    EAOEConfigShape Shape = EAOEConfigShape::Sphere;

    /** [Sphere / Capsule] 반지름 (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AOE|Shape",
        meta = (EditCondition = "Shape == EAOEConfigShape::Sphere || Shape == EAOEConfigShape::Capsule",
                EditConditionHides, ClampMin = "0"))
    float Radius = 200.f;

    /** [Box] 반대각선 크기 (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AOE|Shape",
        meta = (EditCondition = "Shape == EAOEConfigShape::Box",
                EditConditionHides))
    FVector BoxExtent = FVector(150.f, 150.f, 80.f);

    /** [Capsule] 높이 반값 (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AOE|Shape",
        meta = (EditCondition = "Shape == EAOEConfigShape::Capsule",
                EditConditionHides, ClampMin = "0"))
    float CapsuleHalfHeight = 200.f;

    // ── 타이밍 ────────────────────────────────────────────────────

    /** AOE 지속 시간 (초) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AOE|Timing",
        meta = (ClampMin = "0.1"))
    float Lifespan = 1.f;

    /** 조준 최대 거리 (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AOE|Timing",
        meta = (ClampMin = "100"))
    float MaxTargetRange = 1000.f;

    /** 코스팅(캐스팅 루프) 시간 (초). 0 = 즉시 조준 단계진입 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AOE|Timing",
        meta = (ClampMin = "0"))
    float CastTime = 2.f;

    /** 디버그 시각화 (에디터/플레이 모드) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AOE|Debug")
    bool bDebugDraw = false;

    /** 서버 전용 스폰 플래그 (멀티플레이 시 클라에서는 스폰되지 않음) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AOE|Network")
    bool bServerOnly = true;

    /** 스킬이 추가로 적용할 GameplayEffect 리스트 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AOE|Effects")
    TArray<TSubclassOf<UGameplayEffect>> AdditionalEffects;


};

UENUM(BlueprintType)
enum class EJobClass : uint8
{
    None,
    Defender,
    Berserker,
    Cleric
};

UENUM(BlueprintType)
enum class ESkillType : uint8
{
    Active,
    Passive
};

USTRUCT(BlueprintType)
struct FSkillRow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  Id;
    
    // [New] (원래 Name이었음) 스킬 툴팁(UI)에 띄워줄 상세 설명글 (엔터키 줄바꿈 가능)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true")) 
    FText Description;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ESkillType Type = ESkillType::Active;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EJobClass  AllowedClass = EJobClass::Defender;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  MaxLevel = 3;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  RequiredCharacterLevel = 1;
    
    // [New] 실제 유저 화면 아이콘 밑에 뜰 "멋진 스킬 이름"
    UPROPERTY(EditAnywhere, BlueprintReadOnly) 
    FText DisplayName;


    // 기본 쿨타임(초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float Cooldown = 0.f;
    // 레벨당 추가 쿨타임(선택, 필요 없으면 0)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float CooldownPerLevel = 0.f;
    // 에디터에서 드롭해둘 아이콘(소프트 레퍼런스 권장)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Icon;
    // 액티브: 공용 GA (예: GA_Melee_Generic)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides)) 
    TSubclassOf<UGameplayAbility> AbilityClass;
    
    // 애니/수치 파라미터(선택)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides)) 
    TObjectPtr<UAnimMontage> Montage;       // 격발(Release) 애니바 

    /** [GroundTarget 전용] 코스팅 루프 애니바 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active && bIsGroundTarget", EditConditionHides))
    TObjectPtr<UAnimMontage> CastingMontage;  // 쿨시 대기 반복 애니바

    /** 이 스킬은 Ground Targeting 스타일인가? (체크 하면 AOEConfig 코스팅 옵션이 보임) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    bool bIsGroundTarget = false;

    /** [GroundTarget 전용] AOE / 케스팅 시간 / 데칼 설정 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active && bIsGroundTarget", EditConditionHides))
    FAOEConfig AOEConfig;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides)) 
    TArray<float> LevelScalars;       // 레벨별 계수(데미지 등)
    
    // [New] 이 스킬이 기절(Stun) 등의 상태 이상을 유발하는가?
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides)) 
    bool bHasStun = false;

    // [New] 레벨별 스턴/CC 시간 (액티브 + bHasStun 체크 시에만 에디터에 보임!)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active && bHasStun", EditConditionHides)) 
    TArray<float> StunDurations;
    
    // 패시브: 공용 GE 템플릿(무한 지속, SetByCaller 또는 스택)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Passive", EditConditionHides)) 
    TSubclassOf<UGameplayEffect> PassiveEffect;
    
    // SetByCaller 키(패시브/액티브 공통으로 쓰고 싶으면)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SetByCallerKey = "Data.SkillValue";

    /** 기본 스태미나 소모량 (1레벨 기준) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    float StaminaCost = 0.f;

    /** 레벨당 추가 소모량 (옵션) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    float StaminaCostPerLevel = 0.f;

    // --- 선행 스킬 (Skill Tree) ---
    // [New] 선행 스킬이 존재하는가?
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prerequisite")
    bool bHasPrerequisite = false;

    /** 이 스킬을 배우기 위해 필요한 선행 스킬 ID */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prerequisite", meta = (EditCondition = "bHasPrerequisite", EditConditionHides))
    FName PrerequisiteSkillId = NAME_None;

    /** 선행 스킬 요구 레벨 (기본 1) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prerequisite", meta = (EditCondition = "bHasPrerequisite", EditConditionHides))
    int32 PrerequisiteSkillLevel = 1;
};



UCLASS(BlueprintType)
class USkillDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<FName, FSkillRow> Skills; // Id → 정의
};
