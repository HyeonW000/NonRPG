#pragma once

#include "CoreMinimal.h"
#include "Character/EnemyCharacter.h"
#include "GameplayAbilitySpecHandle.h"
#include "BossCharacter.generated.h"

class USphereComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossPhaseChanged, int32, NewPhase);

class UBossDataAsset;
class UGameplayAbility;

UCLASS()
class NON_API ABossCharacter : public AEnemyCharacter
{
    GENERATED_BODY()

public:
    ABossCharacter();

    UPROPERTY(BlueprintAssignable, Category = "Boss|Events")
    FOnBossPhaseChanged OnBossPhaseChanged;

protected:
    virtual void BeginPlay() override;
    virtual void UpdateHPBar() const override;

public:
    // 데미지 무시(무적) 처리를 위한 오버라이드
    virtual void ApplyDamageAt(float Amount, AActor* DamageInstigator, const FVector& WorldLocation, bool bIsCritical = false, FGameplayTag ReactionTag = FGameplayTag()) override;

    // 데미지 숫자 위치 스폰 보정용 오버라이드
    virtual void Multicast_SpawnDamageNumber_Implementation(float Amount, FVector WorldLocation, bool bIsCritical) override;

    // 데이터에셋 정보 초기화 오버라이드 (BossDataAsset 사용)
    UFUNCTION(BlueprintCallable, Category = "Boss|Config")
    void InitBossData(const UBossDataAsset* InBossData);

    // 보스 페이즈 데이터 초기화 (PhaseList 적용)
    UFUNCTION(BlueprintCallable, Category = "Boss|Config")
    void InitBossPhaseData(const UBossDataAsset* InBossPhaseData);
    
public:
    // 보스 인지 반경 (거리에 가까워지면 보스 UI가 나타남)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|UI")
    USphereComponent* UIActivationZone;

    // BossData (DT_EnemyData의 BossPhaseData로부터 자동 주입 - BP 기본값에서는 숨김)
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss|Data")
    const UBossDataAsset* BossData;

    // 현재 페이즈 (1부터 시작)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Phase")
    int32 CurrentPhase;

    // 페이즈 전환 중인지 여부 (이때 무적)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Phase")
    bool bIsTransitioningPhase;

    // 현재 페이즈에서 부여받은 스킬들의 핸들 목록 (페이즈 넘어갈 때 해제하기 위함)
    TArray<FGameplayAbilitySpecHandle> PhaseAbilityHandles;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Boss|Phase")
    void ApplyPhase(int32 TargetPhase);

    // 무적 해제 콜백
    UFUNCTION()
    void EndPhaseTransition();

    UFUNCTION()
    void OnPhaseTransitionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    // ───── 보스 부위 파괴 시스템 ─────
    // 에디터에서 Bone 이름(예: tail, head)과 해당 부위의 체력을 설정합니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Destruction")
    TMap<FName, float> PartHealthMap;

    // 파괴된 부위들을 저장
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Destruction")
    TSet<FName> BrokenParts;

    // 부위 파괴 시 호출될 이벤트 (C++ 및 블루프린트 양쪽 100% 대응)
    UFUNCTION(BlueprintNativeEvent, Category = "Boss|Destruction")
    void OnPartBroken(FName BoneName);
    virtual void OnPartBroken_Implementation(FName BoneName);

    // StandAlone / 클라이언트 화면 렌더링 전파용 멀티캐스트 함수
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnPartBroken(FName BoneName);

    // 부위 파괴 시 보스에게 부여할 영구 태그 (예: tail_01 -> State.Broken.Tail)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Destruction")
    TMap<FName, FGameplayTag> PartBreakTags;

    // 부위별 파괴 시 재생할 몽타주 리스트 (에디터에서 설정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Destruction")
    TMap<FName, class UAnimMontage*> PartBreakMontages;

    // 현재 부위 파괴(Pain) 경직 몽타주 재생 중인지 여부
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Destruction")
    bool bIsPlayingPartBreak = false;

    // 주어진 몽타주가 부위 파괴(Pain/Break/Groggy) 몽타주인지 판별
    UFUNCTION(BlueprintPure, Category = "Boss|Destruction")
    bool IsPlayingPartBreakMontage(const class UAnimMontage* Montage) const;

    UFUNCTION()
    void OnPartBreakMontageEnded(class UAnimMontage* Montage, bool bInterrupted);

    UFUNCTION()
    void EndPartBreakReaction();

    FTimerHandle PartBreakTimerHandle;

    // 피격 시 해당 부위 데미지 처리 함수
    void ProcessPartDamage(FName BoneName, float Damage);

    // ───── 보스 그로기 (설계서 5장 5) ─────
    // 패링 / 저스트 회피 직후 반격 / 스킬 데이터 GroggyAmount 로 게이지를 쌓고, 가득 차면 그로기.

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Groggy", meta = (ClampMin = "1.0"))
    float GroggyMax = 100.f;

    /** 그로기 지속 시간 (초). 끝나면 GroggyMontage의 End 섹션으로 넘어감 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Groggy", meta = (ClampMin = "0.5"))
    float GroggyDuration = 5.f;

    /** 그로기 중 받는 피해 배율 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Groggy", meta = (ClampMin = "1.0"))
    float GroggyDamageTakenMultiplier = 1.5f;

    /** 마지막 누적 후 이 시간 동안 새로 쌓이지 않으면 자연 감소 시작 (초) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Groggy", meta = (ClampMin = "0.0"))
    float GroggyDecayDelay = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Groggy", meta = (ClampMin = "0.0"))
    float GroggyDecayPerSecond = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Groggy", meta = (ClampMin = "0.0"))
    float GroggyFromParry = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Groggy", meta = (ClampMin = "0.0"))
    float GroggyFromJustDodgeCounter = 15.f;

    /** 섹션 Start → Loop(자기 자신으로 반복 연결) → End 구성 권장. 섹션이 없으면 한 번만 재생 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Groggy")
    TObjectPtr<class UAnimMontage> GroggyMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Groggy")
    FName GroggyEndSection = TEXT("End");

    UPROPERTY(ReplicatedUsing = OnRep_Groggy, VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Groggy")
    float GroggyValue = 0.f;

    UPROPERTY(ReplicatedUsing = OnRep_Groggy, VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Groggy")
    bool bIsGroggy = false;

    /** 서버: 그로기 누적 (그로기 중/페이즈 전환 중/사망 시 무시) */
    UFUNCTION(BlueprintCallable, Category = "Boss|Groggy")
    void AddGroggy(float Amount, AActor* Source);

    /** 서버: 플레이어 타격 1회 → 스킬 그로기 값 + 저스트 회피 반격 보너스 누적 (NonAttributeSet에서 호출) */
    void OnHitByPlayer(AActor* InstigatorActor);

    float GetGroggyDamageMultiplier() const { return bIsGroggy ? GroggyDamageTakenMultiplier : 1.f; }

    UFUNCTION()
    void OnRep_Groggy();

protected:
    void EnterGroggy();
    void ExitGroggy();
    void FinishGroggyRecovery();
    void TickGroggyDecay();
    void UpdateGroggyHUD() const;

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayGroggy(bool bStart);

    float LastGroggyGainTime = -1.f;
    FTimerHandle GroggyTimerHandle;
    FTimerHandle GroggyDecayTimerHandle;
    static constexpr float GroggyDecayInterval = 0.25f;

public:
    // ───── 보스 개별 스킬 쿨타임 관리 ─────
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Cooldown")
    TMap<FName, double> SkillCooldownMap;

    UFUNCTION(BlueprintPure, Category = "Boss|Cooldown")
    bool IsBossSkillOnCooldown(FName SkillName) const;

    UFUNCTION(BlueprintCallable, Category = "Boss|Cooldown")
    void SetBossSkillCooldown(FName SkillName, float CooldownSeconds);

    // 보스 데이터 주도 스킬/전환 몽타주 멀티플레이 동기화 재생
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayBossSkillMontage(UAnimMontage* Montage, float PlayRate = 1.0f);

    // 보스 제자리 회전(Turn) 몽타주 및 모션 워핑 목표 회전값 멀티플레이 동기화 재생
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayBossTurnMontage(UAnimMontage* Montage, FRotator TargetRotation, FName WarpTargetName);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 현재 어그로 대상 타겟 (클라이언트 시선 처리 및 모션 워핑 동기화용)
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Boss|AI")
    TObjectPtr<AActor> CurrentTargetActor = nullptr;

    // 보스 공격/점프 모션 워핑(LocationTarget) 서버+클라이언트 동기화 갱신 함수
    UFUNCTION(BlueprintCallable, Category = "Boss|MotionWarping")
    void UpdateBossMotionWarpTarget(AActor* TargetActorOverride = nullptr);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_UpdateMotionWarpTarget(FName WarpTargetName, FVector TargetLoc, FRotator TargetRot);

    // 🤼 보스 잡기(Grab) 시도/성공 몽타주 및 손 소켓 (GA_Boss_Grab이 캐릭터에서 직접 읽어옵니다!)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Grab")
    TObjectPtr<UAnimMontage> GrabAttemptMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Grab")
    TObjectPtr<UAnimMontage> GrabSuccessMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Grab")
    FName GrabHandSocketName = FName("hand_r_Socket");

protected:
    UFUNCTION()
    virtual void OnUIZoneOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                                      bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    virtual void OnUIZoneOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    void CheckPhaseTransition();
};
