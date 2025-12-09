#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Skill/SkillTypes.h"                         // EJobClass, FSkillRow, USkillDataAsset
#include "SkillManagerComponent.generated.h"

class UAbilitySystemComponent;

/** 개별 스킬 항목 (FastArray 아이템) */
USTRUCT()
struct FSkillLevelEntry : public FFastArraySerializerItem
{
    GENERATED_BODY()

    UPROPERTY()
    FName SkillId;

    UPROPERTY()
    int32 Level = 0;
};

/* ===================================
 * FastArray: 스킬 레벨 컨테이너
 *  - 복제 + 클라에서 브로드캐스트
 * =================================== */
USTRUCT()
struct FSkillLevelContainer : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FSkillLevelEntry> Items;

    /** 소유 컴포넌트(클라에서 브로드캐스트용) */
    UPROPERTY(NotReplicated)
    class USkillManagerComponent* Owner = nullptr;

    // 조회
    int32 GetLevel(FName Id) const
    {
        for (const FSkillLevelEntry& E : Items)
        {
            if (E.SkillId == Id) return E.Level;
        }
        return 0;
    }

    // 설정(신규/갱신)
    void SetLevel(FName Id, int32 NewLevel)
    {
        for (FSkillLevelEntry& E : Items)
        {
            if (E.SkillId == Id)
            {
                E.Level = NewLevel;
                MarkItemDirty(E);
                return;
            }
        }
        FSkillLevelEntry NewE;
        NewE.SkillId = Id;
        NewE.Level = NewLevel;
        Items.Add(NewE);
        MarkItemDirty(NewE);
    }

    /** 복제 이벤트 훅: 클라에서 UI 갱신 트리거 */
    void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize) { BroadcastAll(); }
    void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize) { BroadcastAll(); }
    void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize) { BroadcastAll(); }

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<FSkillLevelEntry, FSkillLevelContainer>(Items, DeltaParms, *this);
    }

private:
    void BroadcastAll();
};

template<>
struct TStructOpsTypeTraits<FSkillLevelContainer> : public TStructOpsTypeTraitsBase2<FSkillLevelContainer>
{
    enum { WithNetDeltaSerializer = true };
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillLevelChanged, FName, SkillId, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillPointsChanged, int32, NewPoints);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJobChanged, EJobClass, NewJob);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSkillCooldownStarted, FName, SkillId, float, Duration, float, EndTime);

/* ===========================
 * 스킬 매니저 컴포넌트
 *  - 직업 / 스킬포인트 / 레벨(복제)
 *  - 액티브: GA 부여 + 레벨 업데이트
 *  - 패시브: GE 적용
 * =========================== */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class USkillManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USkillManagerComponent();

    /* ---------- 초기화 ---------- */

    /** 초기화(서버에서 호출 권장) */
    UFUNCTION(BlueprintCallable)
    void Init(EJobClass InClass, USkillDataAsset* InData, UAbilitySystemComponent* InASC);

    /* ---------- 레벨 / 포인트 ---------- */

    /** 배우기/레벨업 시도(클라→서버 자동 라우팅) */
    UFUNCTION(BlueprintCallable)
    bool TryLearnOrLevelUp(FName SkillId);

    /** 가능 여부 확인(클라에서도 사용 가능) */
    UFUNCTION(BlueprintPure)
    bool CanLevelUp(FName SkillId, FString& OutWhy) const;

    /** 현재 레벨 조회 */
    UFUNCTION(BlueprintPure)
    int32 GetSkillLevel(FName SkillId) const;

    /** 스킬 포인트 증감(서버) */
    UFUNCTION(BlueprintCallable)
    void AddSkillPoints(int32 Delta);

    UFUNCTION(BlueprintPure)
    int32 GetSkillPoints() const { return SkillPoints; }

    /* ---------- 쿨타임 / 사용 ---------- */

    /** 쿨타임 조회 */
    UFUNCTION(BlueprintCallable, BlueprintPure)
    bool IsOnCooldown(FName SkillId, float& OutRemaining) const;

    /** 스킬 사용 시도 (쿨타임 체크 + GA 발동 + 쿨타임 시작) */
    UFUNCTION(BlueprintCallable)
    bool TryActivateSkill(FName SkillId);

    // GA_SkillBase에서 읽을 때 사용할 Getter
    FORCEINLINE USkillDataAsset* GetDataAsset() const { return DataAsset; }

    /** 스킬 포인트 강제 설정 (로드용) */
    void SetSkillPoints(int32 NewPoints) { SkillPoints = NewPoints; }

    /** 스킬 레벨 맵 전체 반환 (저장용) */
    TMap<FName, int32> GetSkillLevelMap() const;

    /** 스킬 레벨 맵 복구 (로드용) */
    void RestoreSkillLevels(const TMap<FName, int32>& InMap);

    // GA_SkillBase에서 어떤 SkillId로 실행됐는지 가져갈 때 쓰는 함수
    FName ConsumePendingSkillId()
    {
        const FName Result = PendingSkillId;
        PendingSkillId = NAME_None;
        return Result;
    }

    /* ---------- 직업 ---------- */

    /** 직업 Getter/Setter */
    UFUNCTION(BlueprintPure)
    EJobClass GetJobClass() const { return JobClass; }

    /** 서버/클라 공용(서버 권한 권장) */
    UFUNCTION(BlueprintCallable)
    void SetJobClass(EJobClass InJob);

    /** 클라→서버 RPC: 직업 변경 */
    UFUNCTION(Server, Reliable)
    void ServerSetJobClass(EJobClass NewJob);

    /* ---------- 델리게이트 (UI 바인딩용) ---------- */

    UPROPERTY(BlueprintAssignable)
    FOnSkillLevelChanged OnSkillLevelChanged;

    UPROPERTY(BlueprintAssignable)
    FOnSkillPointsChanged OnSkillPointsChanged;

    UPROPERTY(BlueprintAssignable)
    FOnSkillCooldownStarted OnSkillCooldownStarted;

    UPROPERTY(BlueprintAssignable)
    FOnJobChanged OnJobChanged;

    //stamina
    float GetStaminaCost(const FSkillRow& Row, int32 Level) const;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

    /** 직업 변경 복제 훅 */
    UFUNCTION()
    void OnRep_JobClass();

    /* ---------- 서버 전용 내부 처리 ---------- */

    /** 서버: 배우기/레벨업 처리 */
    UFUNCTION(Server, Reliable)
    void Server_TryLearnOrLevelUp(FName SkillId);

    /** 액티브 스킬 적용: GA 부여/업데이트 */
    void ApplyActive_GiveOrUpdate(const FSkillRow& Row, int32 NewLevel);

    /** 패시브 스킬 적용: GE 적용(SetByCaller 또는 스택) */
    void ApplyPassive_ApplyOrStack(const FSkillRow& Row, int32 NewLevel);

    /* ---------- 레퍼런스 ---------- */


    UPROPERTY(EditDefaultsOnly, Category = "Skill")
    TMap<EJobClass, USkillDataAsset*> DefaultDataAssets;

    /** 스킬 정의 DataAsset */
    UPROPERTY()
    TObjectPtr<USkillDataAsset> DataAsset = nullptr;

    /** ASC (GA/GE 적용용) */
    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> ASC = nullptr;

    /** FastArray 컨테이너에서 소유자 접근 허용 */
    friend struct FSkillLevelContainer;

private:
    /* ---------- 복제되는 상태 ---------- */

    /** 직업(복제) */
    UPROPERTY(ReplicatedUsing = OnRep_JobClass)
    EJobClass JobClass = EJobClass::Defender;

    /** 스킬 포인트(복제) */
    UPROPERTY(Replicated)
    int32 SkillPoints = 0;

    // 방금 활성화 시도한 스킬 ID
    FName PendingSkillId = NAME_None;

    /** 스킬별 쿨타임 종료 시각(월드 타임 Seconds 기준) */
    UPROPERTY(VisibleInstanceOnly, Category = "Skill|Cooldown")
    TMap<FName, float> CooldownEndTimes;

    /** 스킬 레벨 컨테이너(복제) */
    UPROPERTY(Replicated)
    FSkillLevelContainer SkillLevels;
};