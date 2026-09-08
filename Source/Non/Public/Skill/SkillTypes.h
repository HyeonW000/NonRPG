#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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

// ─────────────────────────────────────────────────────────────────
// 세부 서브 구조체 (에디터 삼각형 ▶ 접고 펼치기 그룹)
// ─────────────────────────────────────────────────────────────────

/** 1. 자원 소모 및 쿨타임 설정 */
USTRUCT(BlueprintType)
struct FSkillCostInfo
{
    GENERATED_BODY()

    /** 소모할 자원 종류 (SP, MP, HP) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ESkillCostType CostType = ESkillCostType::SP;

    /** 기본 자원 소모량 (1레벨 기준) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float CostValue = 0.f;

    /** 레벨당 추가 소모량 (옵션) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float CostValuePerLevel = 0.f;

    /** 기본 쿨타임 (초) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Cooldown = 0.f;

    /** 레벨당 추가 쿨타임 (초) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float CooldownPerLevel = 0.f;
};

/** 2. 어빌리티 및 애니메이션 설정 */
USTRUCT(BlueprintType)
struct FSkillCombatInfo
{
    GENERATED_BODY()

    /** 액티브: 공용 GA (예: GA_Melee_Generic, GA_Berserker_Skill 등) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UGameplayAbility> AbilityClass;

    /** 격발(Release) 애니메이션 몽타주 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UAnimMontage> Montage;

    /** 레벨별 계수 (데미지 배율 등) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<float> LevelScalars;

    /** [선택] 풀바디 모션 강제 여부 (기본 true = 전신 몽타주 재생, false = 상체 블렌딩 몽타주로 이동 중 시전 가능!) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bForceFullBody = true;
};

/** 3. 캐스팅 및 조준 설정 */
USTRUCT(BlueprintType)
struct FSkillCastingInfo
{
    GENERATED_BODY()

    /** 시전/캐스팅 시간 (초). 0 = 즉시 시전 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
    float CastTime = 0.f;

    /** 캐스팅/시전 루프 애니메이션 (Cast_start -> Cast_Idle(Loop)) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UAnimMontage> CastingMontage;

    /** [발사체 전용] 스폰할 발사체(Projectile) 클래스 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<class AActor> ProjectileClass;

    /** 이 스킬은 Ground Targeting 장판 조준 스타일인가? */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bIsGroundTarget = false;

    /** [GroundTarget 전용] AOE / 데칼 설정 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bIsGroundTarget", EditConditionHides))
    FAOEConfig AOEConfig;
};

/** 4. 버프 설정 */
USTRUCT(BlueprintType)
struct FSkillBuffInfo
{
    GENERATED_BODY()

    /** 이 스킬이 자신 또는 아군에게 버프를 부여하는 스킬인가? */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasBuff = false;

    /** 시전 시 자신 및 아군에게 부여할 버프 게임플레이 이펙트 (예: GE_Berserk, GE_WarCry 등) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bHasBuff", EditConditionHides))
    TSubclassOf<UGameplayEffect> BuffEffect;

    /** 버프 적용 반경 (cm 단위, 0 = 나 자신만 적용, 1000 = 주변 10m 내 아군/파티원에게도 광역 적용!) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bHasBuff", EditConditionHides))
    float BuffRadius = 0.f;

    /** [New] 레벨별 버프 지속 시간 (초). 설정되어 있으면 GE의 기본 지속시간 대신 이 값을 우선 적용합니다 (예: 1레벨 10초, 2레벨 12초, 3레벨 14초) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bHasBuff", EditConditionHides))
    TArray<float> BuffDurations;
};

/** 5. 상태이상 및 군중제어(CC) 설정 */
USTRUCT(BlueprintType)
struct FSkillStatusEffectInfo
{
    GENERATED_BODY()

    /** 이 스킬이 기절(Stun) 상태이상을 유발하는가? */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasStun = false;

    /** 레벨별 스턴/CC 시간 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bHasStun", EditConditionHides))
    TArray<float> StunDurations;

    /** 이 스킬이 지속 피해/상태이상(화상, 출혈 등)을 유발하는가? */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasStatusEffect = false;

    /** 레벨별 상태이상 지속시간 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bHasStatusEffect", EditConditionHides))
    TArray<float> StatusEffectDurations;

    /** 레벨별 상태이상 발동 확률 (0.0 ~ 1.0 범위, 예: 0.1이면 10%) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bHasStatusEffect", EditConditionHides))
    TArray<float> StatusEffectChances;

    /** 레벨별 상태이상 수치/계수 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bHasStatusEffect", EditConditionHides))
    TArray<float> StatusEffectValues;
};

/** 6. 연계 콤보 및 특수 상태 설정 */
USTRUCT(BlueprintType)
struct FSkillComboInfo
{
    GENERATED_BODY()

    /** 1단계 스킬 시전 성공 후 동일 단축키 연타 시 발동할 2단계 연계 스킬 ID */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName NextComboSkillId = NAME_None;

    /** 연계 가능 대기 시간 (초) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float ComboWindowDuration = 3.0f;

    /** 🛡️ 연계 전용 스킬 여부 (true 면 선행 스킬 후 콤보 창이 열렸을 때만 발동 가능, 단독 시전 불가!) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bIsComboOnlySkill = false;

    /** 🔥 분노 상태(State.Rage) 전용 스킬 여부 (true 면 분노 상태일 때만 발동 가능!) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bRequiresRageState = false;
};

/** 7. 선행 스킬 요구조건 설정 */
USTRUCT(BlueprintType)
struct FSkillPrerequisiteInfo
{
    GENERATED_BODY()

    /** 선행 스킬이 존재하는가? */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasPrerequisite = false;

    /** 이 스킬을 배우기 위해 필요한 선행 스킬 ID */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bHasPrerequisite", EditConditionHides))
    FName PrerequisiteSkillId = NAME_None;

    /** 선행 스킬 요구 레벨 (기본 1) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bHasPrerequisite", EditConditionHides))
    int32 PrerequisiteSkillLevel = 1;
};

/** 8. 패시브 전용 설정 (100% GAS GameplayEffect 표준) */
USTRUCT(BlueprintType)
struct FSkillPassiveInfo
{
    GENERATED_BODY()

    /** 패시브 게임플레이 이펙트 (무한 지속 Infinite GE - 스탯 증가, 특성 태그 부여 등) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UGameplayEffect> PassiveEffect;

    /** [선택] 조건부 발동형 이펙트 (예: 흡혈 시 즉발 힐 GE, 반격 GE 등) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UGameplayEffect> TriggerEffect;

    /** SetByCaller 수치 주입 태그 키 (기본: Data.SkillValue) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName SetByCallerKey = "Data.SkillValue";

    /** [선택] 특정 버프/상태 태그의 지속시간을 증가시키는 패시브인 경우 대상 태그 (예: State.Rage, State.Stealth 등) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "GameplayTag"))
    FGameplayTag TargetDurationTag;

    /** 레벨별 수치/계수 배열 (Lv1, Lv2, Lv3... - 지속시간 증가 초 또는 배율) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<float> PassiveValues;
};

// ─────────────────────────────────────────────────────────────────
// 메인 스킬 데이터 행 구조체
// ─────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FSkillRow
{
    GENERATED_BODY()

    // ── 기본 정보 (Basic Info - 에디터에서 즉시 확인 가능) ──
    UPROPERTY(EditAnywhere, BlueprintReadOnly) 
    FName Id;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly) 
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true")) 
    FText Description;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly) 
    ESkillType Type = ESkillType::Active;

    UPROPERTY(EditAnywhere, BlueprintReadOnly) 
    EJobClass AllowedClass = EJobClass::Defender;

    UPROPERTY(EditAnywhere, BlueprintReadOnly) 
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly) 
    int32 MaxLevel = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly) 
    int32 RequiredCharacterLevel = 1;

    /** 스킬 레벨별 요구 캐릭터 레벨 배열 - [Lv1 요구레벨, Lv2 요구레벨...] */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<int32> RequiredCharacterLevelPerLevel;

    // ── 서브 구조체 그룹들 (삼각형 ▶ 접고 펼치기) ──
    /** 1. 소모 자원 및 쿨타임 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    FSkillCostInfo Cost;

    /** 2. 어빌리티 및 몽타주 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    FSkillCombatInfo Combat;

    /** 3. 캐스팅 및 조준 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    FSkillCastingInfo Casting;

    /** 4. 버프 시스템 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    FSkillBuffInfo Buff;

    /** 5. 상태이상 및 CC기 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FSkillStatusEffectInfo StatusEffect;

    /** 6. 콤보 및 특수 상태 (분노) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Active", EditConditionHides))
    FSkillComboInfo Combo;

    /** 7. 선행 스킬 요구조건 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FSkillPrerequisiteInfo Prerequisite;

    /** 8. 패시브 전용 설정 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Type == ESkillType::Passive", EditConditionHides))
    FSkillPassiveInfo Passive;
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
