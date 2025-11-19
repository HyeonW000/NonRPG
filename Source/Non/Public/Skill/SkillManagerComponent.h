#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Skill/SkillTypes.h"                         // EJobClass, FSkillRow, USkillDataAsset
#include "Net/Serialization/FastArraySerializer.h"    // FastArray
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

/** 스킬 레벨 컨테이너 (FastArray 컨테이너) */
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

/**
 * 스킬 매니저
 * - 직업/스킬 포인트/스킬 레벨(복제)
 * - 액티브: GA 부여/레벨 갱신
 * - 패시브: GE 적용(SetByCaller or 스택)
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class USkillManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USkillManagerComponent();

    /** 초기화(서버에서 호출 권장) */
    UFUNCTION(BlueprintCallable)
    void Init(EJobClass InClass, USkillDataAsset* InData, UAbilitySystemComponent* InASC);

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

    /** 직업 Getter/Setter */
    UFUNCTION(BlueprintPure)
    EJobClass GetJobClass() const { return JobClass; }

    UFUNCTION(BlueprintCallable)
    void SetJobClass(EJobClass InJob);        // 서버/클라 공용(서버에서 권한 O)

    /** UI등 바인딩용 이벤트 */
    UPROPERTY(BlueprintAssignable) FOnSkillLevelChanged  OnSkillLevelChanged;
    UPROPERTY(BlueprintAssignable) FOnSkillPointsChanged OnSkillPointsChanged;
    UPROPERTY(BlueprintAssignable) FOnJobChanged         OnJobChanged;

    /** 클라→서버 RPC: 직업 변경 */
    UFUNCTION(Server, Reliable)
    void ServerSetJobClass(EJobClass NewJob);

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** 직업(복제) */
    UPROPERTY(ReplicatedUsing = OnRep_JobClass)
    EJobClass JobClass = EJobClass::Defender;

    /** 포인트(복제) */
    UPROPERTY(Replicated)
    int32 SkillPoints = 0;

    /** 스킬 레벨 컨테이너(복제) */
    UPROPERTY(Replicated)
    FSkillLevelContainer SkillLevels;

    /** 직업 변경 복제 훅 */
    UFUNCTION()
    void OnRep_JobClass();

    /** 서버 전용: 배우기/레벨업 처리 */
    UFUNCTION(Server, Reliable)
    void Server_TryLearnOrLevelUp(FName SkillId);

    /** 적용 루틴 */
    void ApplyActive_GiveOrUpdate(const FSkillRow& Row, int32 NewLevel);
    void ApplyPassive_ApplyOrStack(const FSkillRow& Row, int32 NewLevel);

    /** 레퍼런스 */
    UPROPERTY(EditAnywhere)
    TObjectPtr<USkillDataAsset> DataAsset = nullptr;

    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> ASC = nullptr;

    friend struct FSkillLevelContainer; // BroadcastAll 접근
};
