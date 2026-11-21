#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Character/EnemyCharacter.h"
#include "Data/BossDataAsset.h"
#include "EnemyDataAsset.generated.h"

class AActor;
class UGameplayAbility;

// 개별 드롭 아이템 정보
USTRUCT(BlueprintType)
struct FEnemyDropItem
{
    GENERATED_BODY()

    // DT_ItemData 안의 RowName (아이템 ID)
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ItemId;

    // 드롭 확률 (0.0 ~ 1.0)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DropChance = 0.5f;

    // 드롭 개수 범위
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MinCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaxCount = 1;
};

/**
 * 엑셀(DataTable) 표 하나로 모든 몬스터의 스탯과 드랍템을 일괄 관리할 수 있는 행 구조체 (FEnemyDataTableRow)
 */
USTRUCT(BlueprintType)
struct NON_API FEnemyDataTableRow : public FTableRowBase
{
    GENERATED_BODY()

    // 에디터 식별용 ID (RowName과 일치 권장)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ID")
    FName EnemyId;

    // UI 표시용 몬스터 이름 (한글)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FText Name;

    // 어떤 Enemy BP를 소환할지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class")
    TSubclassOf<AEnemyCharacter> EnemyClass;

    // 기본 스탯
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    float MaxHP = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    float Attack = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    float Defense = 5.f;

    // 기본 공격 사정거리 (단검/맨손: 220, 검/창: 300, 대형/보스: 400+)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (EditCondition = "!bIsBoss && CombatStyle != EEnemyCombatStyle::Ranged", EditConditionHides))
    float AttackRange = 220.f;

    // 공격 속성 타입 (Physical: 물리, Magical: 마법 등)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    ENonDamageType AttackDamageType = ENonDamageType::Physical;

    // 공격력 계수/배율 (1.0 = 공격력의 100% 데미지, 1.5 = 150% 데미지)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
    float AttackPowerScale = 1.0f;

    // 전투 공격 스타일 (근접 전용 / 원거리 전용 / 근접+원거리 하이브리드)
    // 스타일에 맞춰 아래의 사정거리, 쿨타임, 몽타주 항목이 필요한 것만 엑셀처럼 깔끔하게 표시됩니다!
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (EditCondition = "!bIsBoss", EditConditionHides))
    EEnemyCombatStyle CombatStyle = EEnemyCombatStyle::Melee;

    // 사정거리 첫 진입 시 공격 뜸들이기 대기 시간 최소/최대 (초 단위, 기본 0.5초 ~ 1.0초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (EditCondition = "!bIsBoss", EditConditionHides, ClampMin = "0.0"))
    float FirstAttackWindupMin = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (EditCondition = "!bIsBoss", EditConditionHides, ClampMin = "0.0"))
    float FirstAttackWindupMax = 1.0f;

    // 기본(근접) 공격 쿨타임 (초 단위, 공격 시전 후 다음 근접 공격까지 쉬는 텀, 기본 1.5초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Cooldown", meta = (EditCondition = "!bIsBoss && CombatStyle != EEnemyCombatStyle::Ranged", EditConditionHides, ClampMin = "0.0"))
    float AttackCooldown = 1.5f;

    // 원거리 공격 쿨타임 (초 단위, 공격 시전 후 다음 원거리 공격까지 쉬는 텀, 기본 4.0초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Cooldown", meta = (EditCondition = "!bIsBoss && CombatStyle != EEnemyCombatStyle::Melee", EditConditionHides, ClampMin = "0.0"))
    float RangeAttackCooldown = 4.0f;

    // AI 어그로 성향 (Preemptive: 선공, Reactive: 비선공/피격 시 반격)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
    EAggroStyle AggroStyle = EAggroStyle::Preemptive;

    // 전투 시 추적/달리기 속도 (기본 400.f, 빠른 늑대: 550.f, 느린 골렘: 250.f)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
    float CombatRunSpeed = 400.f;

    // 평상시 순찰 걷기 속도 (기본 200.f)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
    float PatrolWalkSpeed = 200.f;

    // 조준/선회 회전 속도 (초당 각도, 기본 220.0f - 낮을수록 묵직하고 느리게 회전)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "60.0", ClampMax = "720.0"))
    float AimTurnRate = 220.0f;

    // 처치 시 경험치
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
    float ExpReward = 10.f;

    // 드랍 아이템 목록
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drops")
    TArray<FEnemyDropItem> DropList;

    // 보스 몬스터 여부 (체크 시 보스 전용 페이즈 설정만 나타나고 일반 평타 몽타주는 완전히 숨겨집니다)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
    bool bIsBoss = false;

    // 공격 애니메이션 몽타주 목록 (일반 몬스터 전용 - GA_EnemyAttack이 이 중 하나를 랜덤 재생!)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (EditCondition = "!bIsBoss && CombatStyle != EEnemyCombatStyle::Ranged", EditConditionHides))
    TArray<TObjectPtr<class UAnimMontage>> AttackMontages;

    // 원거리 공격 애니메이션 몽타주 (선택 사항 - 활/마법 등)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (EditCondition = "!bIsBoss && CombatStyle != EEnemyCombatStyle::Melee", EditConditionHides))
    TObjectPtr<class UAnimMontage> RangeAttackMontage = nullptr;

    // 원거리 공격 사정거리 (기본 1200cm = 12m)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Attack", meta = (EditCondition = "!bIsBoss && CombatStyle != EEnemyCombatStyle::Melee", EditConditionHides, ClampMin = "0.0"))
    float RangeAttackRange = 1200.f;

    // 사망 애니메이션 몽타주 (일반 몬스터 전용 - GA_Death가 사망 시 재생!)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (EditCondition = "!bIsBoss", EditConditionHides))
    TObjectPtr<class UAnimMontage> DeathMontage = nullptr;

    // 약피격 애니메이션 몽타주 (일반 몬스터 전용 - 기본 평타 맞았을 때)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Hit", meta = (EditCondition = "!bIsBoss", EditConditionHides))
    TObjectPtr<class UAnimMontage> HitLightMontage = nullptr;

    // 강피격 애니메이션 몽타주 (일반 몬스터 전용 - 강공격 맞았을 때, 비어있으면 약피격으로 자동 대체)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Hit", meta = (EditCondition = "!bIsBoss", EditConditionHides))
    TObjectPtr<class UAnimMontage> HitHeavyMontage = nullptr;

    // 넉다운(다운) 애니메이션 몽타주 (일반 몬스터 전용 - 넘어짐 피격, 비어있으면 약피격으로 자동 대체)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Hit", meta = (EditCondition = "!bIsBoss", EditConditionHides))
    TObjectPtr<class UAnimMontage> KnockdownMontage = nullptr;

    // 스턴(기절) 애니메이션 몽타주 (일반 몬스터 전용 - 그로기/스턴 상태, 비어있으면 약피격으로 자동 대체)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Hit", meta = (EditCondition = "!bIsBoss", EditConditionHides))
    TObjectPtr<class UAnimMontage> StunMontage = nullptr;

    // 선공(Preemptive) 최초 발견 시 재생할 경계/포효 애니메이션 몽타주 (AI 경계 연출)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
    TObjectPtr<class UAnimMontage> AlertMontage = nullptr;

    // 선공(Preemptive) 최초 발견 시 머리 위 경계 효과(느낌표/효과선 위젯)를 띄울지 여부 (기본 true, 보스는 보통 false 추천)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
    bool bShowAlertMark = true;

    // 선공(Preemptive) 최초 발견 시 플레이어를 주시하며 멈칫거리는 경계 텀(대기 시간, 초 단위, 기본 0.8초)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0.0"))
    float AlertDelay = 0.8f;

    // 플레이어가 너무 가까이 접근했을 때 뒤로 물러나 거리를 벌리는 카이팅/도망 여부
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Flee", meta = (EditCondition = "!bIsBoss && CombatStyle != EEnemyCombatStyle::Melee", EditConditionHides))
    bool bFleeWhenTooClose = false;

    // 뒤로 물러날 때 재생할 백스텝 회피 몽타주 (선택 사항 - 비어있으면 뒤로 자연스럽게 이동하여 거리를 벌립니다)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Flee", meta = (EditCondition = "!bIsBoss && CombatStyle != EEnemyCombatStyle::Melee && bFleeWhenTooClose", EditConditionHides))
    TObjectPtr<class UAnimMontage> BackstepMontage = nullptr;

    // 보스 전용 페이즈 데이터 에셋 (bIsBoss가 true일 때만 나타납니다)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss", meta = (EditCondition = "bIsBoss", EditConditionHides))
    TObjectPtr<const UBossDataAsset> BossPhaseData = nullptr;
};

UCLASS(BlueprintType)
class NON_API UEnemyDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    // 에디터에서 식별용 이름(옵션)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ID")
    FName EnemyId;

    // [New] UI 표시용 이름 (한글 가능)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FText Name;

    // 어떤 Enemy BP를 스폰할지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class")
    TSubclassOf<AEnemyCharacter> EnemyClass;

    //  이 적이 참조할 아이템 데이터 테이블 (DT_ItemData)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drops")
    UDataTable* ItemDataTable;

    //  어떤 아이템을 드롭할지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drops")
    TArray<FEnemyDropItem> DropList;

    // 기본 스탯
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    float MaxHP = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    float Attack = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    float Defense = 5.f;

    // 처치 시 경험치
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
    float ExpReward = 10.f;
};