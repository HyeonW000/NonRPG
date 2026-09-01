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

UENUM(BlueprintType)
enum class EGroundTargetType : uint8
{
    CastThenTarget              UMETA(DisplayName = "Cast Then Manual Target (기존 수동)"),
    SimultaneousCastAndTarget   UMETA(DisplayName = "Simultaneous Cast and Target (동시 조준)"),
    InstantAoE                  UMETA(DisplayName = "Instant AoE (즉발 장판)"),
    SimultaneousCastThenClick   UMETA(DisplayName = "Simultaneous Cast Then Click (동시 조준 후 클릭)")
};

UENUM(BlueprintType)
enum class ESkillCostType : uint8
{
    SP    UMETA(DisplayName = "Stamina (SP)"),
    MP    UMETA(DisplayName = "Mana (MP)"),
    HP    UMETA(DisplayName = "Health (HP)")
};

UENUM(BlueprintType)
enum class ESkillType : uint8
{
    Active UMETA(DisplayName = "Active"),
    Passive UMETA(DisplayName = "Passive")
};

UENUM(BlueprintType)
enum class EPassiveType : uint8
{
    None            UMETA(DisplayName = "일반 패시브 (기존 GE 방식)"),
    BerserkerRage   UMETA(DisplayName = "버서커 - 광전사"),
    Bloodthirst     UMETA(DisplayName = "버서커 - 피의 갈증"),
    Rage            UMETA(DisplayName = "버서커 - 분노"),
    RageMastery     UMETA(DisplayName = "버서커 - 분노 숙련")
};

USTRUCT(BlueprintType)
struct FRageStageBonus
{
    GENERATED_BODY()

    /** 물리 공격력 보너스 (%) - [Lv1, Lv2, Lv3] */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BerserkerRage")
    TArray<float> AttackBonusPerLevel = { 15.f, 20.f, 25.f };

    /** 치명타 확률 보너스 (%) - [Lv1, Lv2, Lv3] */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BerserkerRage")
    TArray<float> CritBonusPerLevel = { 3.f, 5.f, 7.f };
};

USTRUCT(BlueprintType)
struct FBloodthirstBonus
{
    GENERATED_BODY()

    /** 레벨별 치명타 시 피흡 발동 확률 (%) - [Lv1: 30%, Lv2: 50%] */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bloodthirst")
    TArray<float> ChancePerLevel = { 30.f, 50.f };

    /** 레벨별 내 MaxHP 피흡 회복 비율 (%) - [Lv1: 1%, Lv2: 2%] */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bloodthirst")
    TArray<float> HealPercentPerLevel = { 1.f, 2.f };
};

USTRUCT(BlueprintType)
struct FAOEConfig
{
    GENERATED_BODY()

    /** 조준/시전 타입 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AOE")
    EGroundTargetType TargetType = EGroundTargetType::CastThenTarget;

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
    Cleric,
    Sorcerer
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

    /** 🔒 [Required Character Level Per Skill Level] 스킬 레벨별 요구 캐릭터 레벨 배열 - [Lv1 요구레벨, Lv2 요구레벨...] */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
    TArray<int32> RequiredCharacterLevelPerLevel;
    
    // [New] 실제 유저 화면 아이콘 밑에 뜰 "멋진 스킬 이름"
    UPROPERTY(EditAnywhere, BlueprintReadOnly) 
    FText DisplayName;

    // ----------------------------------------------------
    // [Passive Setup] 패시브 스킬 전용 세팅 (Type 이 Passive 일 때만 완벽 노출!)
    // ----------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passive", meta = (EditCondition = "Type == ESkillType::Passive", EditConditionHides))
    EPassiveType PassiveType = EPassiveType::None;

    /** [광전사의 분노] 1단계 (HP 70% 이하) 수치 세팅 - [레벨별 물공%, 치명%] */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passive|BerserkerRage", meta = (EditCondition = "Type == ESkillType::Passive && PassiveType == EPassiveType::BerserkerRage", EditConditionHides))
    FRageStageBonus RageStage1_HP70 = { {15.f, 20.f, 25.f}, {3.f, 5.f, 7.f} };

    /** [광전사의 분노] 2단계 (HP 40% 이하) 수치 세팅 - [레벨별 물공%, 치명%] */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passive|BerserkerRage", meta = (EditCondition = "Type == ESkillType::Passive && PassiveType == EPassiveType::BerserkerRage", EditConditionHides))
    FRageStageBonus RageStage2_HP40 = { {35.f, 45.f, 60.f}, {8.f, 12.f, 16.f} };

    /** [광전사의 분노] 3단계 (HP 20% 이하 - 광란) 수치 세팅 - [레벨별 물공%, 치명%] */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passive|BerserkerRage", meta = (EditCondition = "Type == ESkillType::Passive && PassiveType == EPassiveType::BerserkerRage", EditConditionHides))
    FRageStageBonus RageStage3_HP20 = { {60.f, 80.f, 100.f}, {15.f, 20.f, 25.f} };

    /** [피의 갈증] 수치 세팅 - [레벨별 발동확률%, MaxHP 회복%] */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passive|Bloodthirst", meta = (EditCondition = "Type == ESkillType::Passive && PassiveType == EPassiveType::Bloodthirst", EditConditionHides))
    FBloodthirstBonus BloodthirstSetup = { {3.f, 6.f, 10.f}, {3.f, 5.f, 7.f} };


    // 기본 쿨타임(초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float Cooldown = 0.f;
    // 레벨당 추가 쿨타임(선택, 필요 없으면 0)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float CooldownPerLevel = 0.f;
    // 에디터에서 드롭해둘 아이콘(소프트 레퍼런스 권장)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Icon;
    // 액티브: 공용 GA (예: GA_Melee_Generic)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides)) 
    TSubclassOf<UGameplayAbility> AbilityClass;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides)) 
    TObjectPtr<UAnimMontage> Montage;       // 격발(Release) 애니바 

    /** [즉발/시전 발사체 전용] 스폰할 발사체(Projectile) 클래스 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    TSubclassOf<class AActor> ProjectileClass;

    /** 시전/캐스팅 시간 (초). 0 = 즉시 시전 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides, ClampMin = "0"))
    float CastTime = 0.f;

    /** 캐스팅/시전 루프 애니메이션 (Cast_start -> Cast_Idle(Loop)) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    TObjectPtr<UAnimMontage> CastingMontage;

    /** 이 스킬은 Ground Targeting 스타일인가? (체크 하면 AOEConfig 옵션이 보임) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    bool bIsGroundTarget = false;

    /** [GroundTarget 전용] AOE / 데칼 설정 */
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

    // [New] 이 스킬이 지속 피해/상태이상(화상, 출혈 등)을 유발하는가?
    UPROPERTY(EditAnywhere, BlueprintReadOnly) 
    bool bHasStatusEffect = false;

    // [New] 레벨별 상태이상 지속시간 (bHasStatusEffect 체크 시에만 보임!)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bHasStatusEffect", EditConditionHides)) 
    TArray<float> StatusEffectDurations;

    // [New] 레벨별 상태이상 발동 확률 (0.0 ~ 1.0 범위, 예: 0.1이면 10%, bHasStatusEffect 체크 시에만 보임!)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bHasStatusEffect", EditConditionHides)) 
    TArray<float> StatusEffectChances;

    // [New] 레벨별 상태이상 수치/계수 (예: 공격력의 5% 도트딜이면 0.05 기입, bHasStatusEffect 체크 시에만 보임!)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bHasStatusEffect", EditConditionHides)) 
    TArray<float> StatusEffectValues;
    
    // 패시브: 공용 GE 템플릿(무한 지속, SetByCaller 또는 스택)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Passive", EditConditionHides)) 
    TSubclassOf<UGameplayEffect> PassiveEffect;
    
    // SetByCaller 키(패시브/액티브 공통으로 쓰고 싶으면)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SetByCallerKey = "Data.SkillValue";

    /** 소모할 자원 종류 (SP, MP, HP) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    ESkillCostType CostType = ESkillCostType::SP;

    /** 기본 자원 소모량 (1레벨 기준) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    float CostValue = 0.f;

    /** 레벨당 추가 소모량 (옵션) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    float CostValuePerLevel = 0.f;

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

    // --- 연계 스킬 시스템 (Combo Chain) ---
    /** 1단계 스킬 시전 성공 후 동일 단축키 연타 시 발동할 2단계 연계 스킬 ID */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo", meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    FName NextComboSkillId = NAME_None;

    /** 연계 가능 대기 시간 (초) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo", meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    float ComboWindowDuration = 3.0f;

    /** 🛡️ [New] 연계 전용 스킬 여부 (true 면 선행 스킬 후 콤보 창이 열렸을 때만 발동 가능, 단독 시전 불가!) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo", meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    bool bIsComboOnlySkill = false;

    /** 🔥 [New] 분노 상태(State.Rage) 전용 스킬 여부 (true 면 분노 상태일 때만 발동 가능!) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rage", meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    bool bRequiresRageState = false;
};



UCLASS(BlueprintType)
class USkillDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    /** 🌟 마우스 드래그앤드롭으로 행 순서를 자유롭게 바꿀 수 있는 스킬 목록 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skills")
    TArray<FSkillRow> SkillList;

    /** 🛡️ C++ 빠른 ID 검색용 맵 (SkillList 배열 기반으로 자동 갱신됨) */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Skills")
    TMap<FName, FSkillRow> Skills;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
    {
        Super::PostEditChangeProperty(PropertyChangedEvent);
        BuildSkillsMap();
    }
#endif

    virtual void PostLoad() override
    {
        Super::PostLoad();
        BuildSkillsMap();
    }

    void BuildSkillsMap()
    {
        Skills.Empty();
        for (const FSkillRow& Row : SkillList)
        {
            if (!Row.Id.IsNone())
            {
                Skills.Add(Row.Id, Row);
            }
        }
    }
};
