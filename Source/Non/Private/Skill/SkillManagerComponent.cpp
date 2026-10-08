#include "Skill/SkillManagerComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "Ability/NonAttributeSet.h"
#include "Character/NonCharacterBase.h"
#include "Core/NonUIManagerComponent.h"
#include "UI/InGameHUD.h"

void USkillManagerComponent::BeginPlay()
{
    Super::BeginPlay();

    // FastArray 컨테이너 소유자 재설정 (안전용)
    SkillLevels.Owner = this;

    // ASC 가 아직 안 들어왔으면, Owner 기준으로 자동으로 찾아본다.
    if (!ASC)
    {
        if (AActor* Owner = GetOwner())
        {
            // 1) 우선 Owner 자신에서 찾기 (캐릭터에 ASC 붙어있는 경우)
            ASC = Owner->FindComponentByClass<UAbilitySystemComponent>();

            // 2) Owner 가 Controller 인 경우 → Pawn 에서 찾기
            if (!ASC)
            {
                if (AController* Ctrl = Cast<AController>(Owner))
                {
                    if (APawn* Pawn = Ctrl->GetPawn())
                    {
                        ASC = Pawn->FindComponentByClass<UAbilitySystemComponent>();
                    }
                }
                // 3) Owner 가 Pawn 인 경우 → Controller 에서도 한 번 더 시도
                else if (APawn* Pawn = Cast<APawn>(Owner))
                {
                    if (AController* PawnCtrl = Pawn->GetController())
                    {
                        ASC = PawnCtrl->FindComponentByClass<UAbilitySystemComponent>();
                    }
                }
            }
        }
    }
}

// ===== FSkillLevelContainer =====
void FSkillLevelContainer::BroadcastAll()
{
    if (!Owner) return;
    for (const FSkillLevelEntry& E : Items)
    {
        Owner->OnSkillLevelChanged.Broadcast(E.SkillId, E.Level);
    }
}

// ===== USkillManagerComponent =====
USkillManagerComponent::USkillManagerComponent()
{
    SetIsReplicatedByDefault(true);
    SkillLevels.Owner = this; //  FastArray 컨테이너에 소유자 연결
}

void USkillManagerComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(USkillManagerComponent, JobClass);
    DOREPLIFETIME(USkillManagerComponent, SkillPoints);
    DOREPLIFETIME(USkillManagerComponent, SkillLevels);
    DOREPLIFETIME(USkillManagerComponent, bIsRageActive);
}

void USkillManagerComponent::OnRep_JobClass()
{
    // [Fix] 클라이언트에서도 DataAsset을 설정해야 함 (안 그러면 UI가 아무것도 못 그림)
    if (USkillDataAsset* const* Found = DefaultDataAssets.Find(JobClass))
    {
        DataAsset = *Found;
    }
    else
    {
    }

    OnJobChanged.Broadcast(JobClass);
}

void USkillManagerComponent::OnRep_SkillPoints()
{
    OnSkillPointsChanged.Broadcast(SkillPoints);
}

void USkillManagerComponent::SetJobClass(EJobClass InJob)
{
    if (JobClass != InJob)
    {
        JobClass = InJob;

        // Job 바뀔 때, DataAsset도 기본 매핑 있으면 같이 교체
        if (USkillDataAsset* const* Found = DefaultDataAssets.Find(JobClass))
        {
            DataAsset = *Found;
        }
        
        // 서버에서도 델리게이트 발동 (Local Server UI 등)
        OnRep_JobClass();
    }
}

void USkillManagerComponent::ServerSetJobClass_Implementation(EJobClass NewJob)
{
    SetJobClass(NewJob);
}

void USkillManagerComponent::Init(EJobClass InClass, USkillDataAsset* InData, UAbilitySystemComponent* InASC)
{
    JobClass = InClass;
    ASC = InASC;
    SkillLevels.Owner = this;

    if (InData)
    {
        // 캐릭터 쪽에서 직접 DataAsset 넘겨줄 경우
        DataAsset = InData;
    }
    else
    {
        // 안 넘겨줬으면 JobClass 기준으로 DefaultDataAssets에서 자동 선택
        if (USkillDataAsset* const* Found = DefaultDataAssets.Find(JobClass))
        {
            DataAsset = *Found;
        }
    }

    // 🔥 [Berserker Class Trait] 버서커 직업일 때 기본 고정 특성 '분노'(B_P_Rage) 레벨 1 자동 부여!
    if (JobClass == EJobClass::Berserker)
    {
        SkillLevels.SetLevel(FName("B_P_Rage"), 1);
        UE_LOG(LogTemp, Warning, TEXT("[SkillManager] Auto Granted 'B_P_Rage' (분노) Class Trait at Level 1 for Berserker!"));
    }

    // 🌟 [GAS 태그 연동] State.Rage 태그 부여/제거를 실시간 감청하여 HUD 버프창 및 퀵슬롯 동기화
    if (ASC)
    {
        static const FGameplayTag RageTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
        ASC->RegisterGameplayTagEvent(RageTag, EGameplayTagEventType::NewOrRemoved)
           .AddUObject(this, &USkillManagerComponent::OnRageTagChanged);
    }
}

void USkillManagerComponent::AddSkillPoints(int32 Delta)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    SkillPoints = FMath::Max(0, SkillPoints + Delta);
    OnSkillPointsChanged.Broadcast(SkillPoints);
}

int32 USkillManagerComponent::GetSkillLevel(FName SkillId) const
{
    return SkillLevels.GetLevel(SkillId);
}

// 스킬 쿨타임 확인
bool USkillManagerComponent::IsOnCooldown(FName SkillId, float& OutRemaining) const
{
    OutRemaining = 0.f;

    const float* EndPtr = CooldownEndTimes.Find(SkillId);
    if (!EndPtr)
        return false;

    const UWorld* World = GetWorld();
    if (!World)
        return false;

    const float Now = World->GetTimeSeconds();
    const float Remaining = *EndPtr - Now;

    if (Remaining <= 0.f)
        return false;

    OutRemaining = Remaining;
    return true;
}

bool USkillManagerComponent::CanLevelUp(FName SkillId, FString& OutWhy) const
{
    bool bResult = false;

    if (!DataAsset) { OutWhy = TEXT("No DataAsset"); }
    else if (const FSkillRow* Row = DataAsset->Skills.Find(SkillId))
    {
        if (Row->AllowedClass != JobClass) { OutWhy = TEXT("Class mismatch"); }
        else
        {
            // 선행 스킬 체크
            if (!Row->Prerequisite.PrerequisiteSkillId.IsNone())
            {
                const int32 PreLevel = SkillLevels.GetLevel(Row->Prerequisite.PrerequisiteSkillId);
                if (PreLevel < Row->Prerequisite.PrerequisiteSkillLevel)
                {
                    OutWhy = FString::Printf(TEXT("Need %s Lv.%d"), *Row->Prerequisite.PrerequisiteSkillId.ToString(), Row->Prerequisite.PrerequisiteSkillLevel);
                    return false;
                }
            }

            const int32 Cur = SkillLevels.GetLevel(SkillId);
            if (Cur >= Row->MaxLevel) { OutWhy = TEXT("Max level"); }
            else if (SkillPoints <= 0) { OutWhy = TEXT("No Skill Points"); }
            else
            {
                // 🔒 요구 캐릭터 레벨 검사 (레벨별 요구치 우선)
                const int32 NextLv = Cur + 1;
                const int32 NextLvIdx = NextLv - 1;
                int32 RequiredCharLevel = Row->RequiredCharacterLevel;
                if (Row->RequiredCharacterLevelPerLevel.IsValidIndex(NextLvIdx))
                {
                    RequiredCharLevel = Row->RequiredCharacterLevelPerLevel[NextLvIdx];
                }

                int32 CurrentCharLevel = 1;
                if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetOwner()))
                {
                    if (const UNonAttributeSet* AS = Char->GetAttributeSet())
                    {
                        CurrentCharLevel = FMath::RoundToInt(AS->GetLevel());
                    }
                }

                if (CurrentCharLevel < RequiredCharLevel)
                {
                    OutWhy = FString::Printf(TEXT("Need Character Lv.%d (Current: %d)"), RequiredCharLevel, CurrentCharLevel);
                    return false;
                }

                OutWhy.Reset();
                bResult = true;
            }
        }
    }
    else
    {
        OutWhy = TEXT("No Skill Row");
    }
    return bResult;
}

bool USkillManagerComponent::TryLearnOrLevelUp(FName SkillId)
{
    if (!GetOwner()) return false;

    FString Why;
    if (!CanLevelUp(SkillId, Why))
    {
        UE_LOG(LogTemp, Warning, TEXT("[SkillUpgrade] Can't upgrade skill %s: %s"), *SkillId.ToString(), *Why);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Can't upgrade skill %s: %s"), *SkillId.ToString(), *Why));
        }
        return false;
    }

    if (GetOwner()->HasAuthority())
    {
        // 서버: 바로 처리
        Server_TryLearnOrLevelUp_Implementation(SkillId);
        return true;
    }

    // 클라면 서버 RPC
    Server_TryLearnOrLevelUp(SkillId);
    return true;
}

void USkillManagerComponent::Server_TryLearnOrLevelUp_Implementation(FName SkillId)
{
    if (!DataAsset) return;

    FString Why;
    if (!CanLevelUp(SkillId, Why)) return;

    const FSkillRow* Row = DataAsset->Skills.Find(SkillId);
    if (!Row) return;

    // 포인트 차감 (Manager)
    SkillPoints = FMath::Max(0, SkillPoints - 1);

    // [Fix] GAS AttributeSet도 동기화 (UI 일치 및 저장 호환성 위해)
    if (ASC)
    {
        const FGameplayAttribute SPAttr = UNonAttributeSet::GetSkillPointAttribute();
        float Current = ASC->GetNumericAttribute(SPAttr);
        ASC->SetNumericAttributeBase(SPAttr, FMath::Max(0.f, Current - 1.f));
    }

    // 레벨 증가
    int32 Cur = SkillLevels.GetLevel(SkillId);
    Cur = FMath::Clamp(Cur + 1, 1, Row->MaxLevel);
    SkillLevels.SetLevel(SkillId, Cur);  // FastArray 수정(자동 Dirty)

    // 액티브/패시브 실제 적용
    if (Row->Type == ESkillType::Active)  ApplyActive_GiveOrUpdate(*Row, Cur);
    else                                   ApplyPassive_ApplyOrStack(SkillId, *Row, Cur);

    // UI용 브로드캐스트
    OnSkillPointsChanged.Broadcast(SkillPoints);
    OnSkillLevelChanged.Broadcast(SkillId, Cur);
}

bool USkillManagerComponent::TryActivateSkill(FName SkillId)
{
    if (!GetOwner()) return false;

    // [Multiplayer Fix] 클라이언트인 경우, 서버에게 스킬 사용 요청 RPC (PendingSkillId 동기화를 위해)
    if (!GetOwner()->HasAuthority())
    {
        // 로컬 예측을 위해 '일단' ASC 호출을 해볼 수도 있지만,
        // GA 내부에서 PendingSkillId를 체크하므로 서버도 반드시 알도록 RPC를 먼저 보내야 함.
        // 여기서는 "RPC 호출 -> 서버가 GA 실행" 흐름으로 통일하거나,
        // Payload에 실어 보내는 방식(ActivateAbilityByEvent)으로 리팩토링이 가장 이상적.
        //
        // 현재 구조 유지(PendingSkillId 사용)를 위해: RPC로 서버에 ID 세팅 후 실행 요청.
        ServerTryActivateSkill(SkillId);
        
        // 클라이언트도 로컬 효과(예측)가 필요하다면 PendingSkillId 세팅 후 TryActivate 호출 가능.
        // 하지만 GA가 'Server Only' 로직이 많다면 굳이 로컬 실행 안 해도 됨.
        // 여기서는 "Local Predicted" GA라면 로컬도 실행해야 함.
        
        // 로컬 실행 (Prediction)
        DoActivateSkillLogic(SkillId); 
        return true;
    }

    // 서버라면 바로 실행
    return DoActivateSkillLogic(SkillId);
}

void USkillManagerComponent::ServerTryActivateSkill_Implementation(FName SkillId)
{
    DoActivateSkillLogic(SkillId);
}

bool USkillManagerComponent::CanActivateSkillNow(FName SkillId) const
{
    if (!DataAsset || !ASC || SkillId.IsNone())
    {
        return false;
    }

    const FSkillRow* Row = DataAsset->Skills.Find(SkillId);
    if (!Row || Row->Type != ESkillType::Active)
    {
        return false;
    }

    // 1. 레벨 미습득 시 false
    if (GetSkillLevel(SkillId) <= 0)
    {
        return false;
    }

    // 2. 쿨타임 중이면 false
    float Remaining = 0.f;
    if (IsOnCooldown(SkillId, Remaining))
    {
        return false;
    }

    // 3. 연계 전용 스킬 조건 (State.Combo.Ready 태그 미보유 시 false -> UI 회색 처리!)
    if (Row->Combo.bIsComboOnlySkill)
    {
        FGameplayTag ReadyTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combo.Ready"), false);
        if (!ASC->HasMatchingGameplayTag(ReadyTag))
        {
            return false;
        }
    }

    // 4. 분노 전용 스킬 조건 (State.Rage 미보유 시 false -> UI 회색 처리!)
    if (Row->Combo.bRequiresRageState)
    {
        static const FGameplayTag RageStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
        if (!ASC->HasMatchingGameplayTag(RageStateTag))
        {
            return false;
        }
    }

    // 5. 🛡️ [직업별 무기 장착 필수 태그 검사 - 버서커, 디펜더, 메이지 등 모든 직업 자동 지원!]
    if (Row->Combat.AbilityClass)
    {
        if (const UGameplayAbility* CDO = Row->Combat.AbilityClass->GetDefaultObject<UGameplayAbility>())
        {
            // 스킬에 요구되는 무기 장착 태그(예: State.Armed.GreatSword) 및 차단 태그 검사
            if (!CDO->DoesAbilitySatisfyTagRequirements(*ASC))
            {
                return false; // 필수 태그 미보유 시 시전 불가 & UI 회색 처리!
            }
        }
    }

    // 6. 자원 부족 시 false
    const float Cost = GetSkillCost(*Row, GetSkillLevel(SkillId));
    if (Cost > 0.f)
    {
        FGameplayAttribute CostAttr;
        if (Row->Cost.CostType == ESkillCostType::SP) CostAttr = UNonAttributeSet::GetSPAttribute();
        else if (Row->Cost.CostType == ESkillCostType::MP) CostAttr = UNonAttributeSet::GetMPAttribute();
        else if (Row->Cost.CostType == ESkillCostType::HP) CostAttr = UNonAttributeSet::GetHPAttribute();

        if (CostAttr.IsValid())
        {
            if (ASC->GetNumericAttribute(CostAttr) < Cost)
            {
                return false;
            }
        }
    }

    return true;
}

bool USkillManagerComponent::DoActivateSkillLogic(FName SkillId)
{
    if (!DataAsset || !ASC)
    {
        return false;
    }

    const FSkillRow* Row = DataAsset->Skills.Find(SkillId);
    if (!Row)
    {
        return false;
    }

    if (Row->Type != ESkillType::Active)
    {
        return false;
    }

    const int32 Level = GetSkillLevel(SkillId);
    if (Level <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ [SkillManager] '%s' 스킬 레벨이 0이어서 시전 실패!"), *SkillId.ToString());
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, FString::Printf(TEXT("⚠️ [%s] 스킬 미습득 (Level: 0)"), *SkillId.ToString()));
        return false;
    }

    float Remaining = 0.f;
    if (IsOnCooldown(SkillId, Remaining))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ [SkillManager] '%s' 스킬 쿨타임 중 (남은 시간: %.1f초)"), *SkillId.ToString(), Remaining);
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Orange, FString::Printf(TEXT("⚠️ [%s] 쿨타임 중 (%.1f초)"), *SkillId.ToString(), Remaining));
        return false;
    }

    // 🛡️ [Fix] 연계 전용 스킬(bIsComboOnlySkill == true) 검사: 콤보 맵에 있거나 State.Combo.Ready 태그가 있을 때 정상 통과!
    if (Row->Combo.bIsComboOnlySkill)
    {
        bool bHasValidCombo = false;
        for (const auto& Pair : ActiveComboChains)
        {
            if (Pair.Value == SkillId)
            {
                bHasValidCombo = true;
                break;
            }
        }

        FGameplayTag ReadyTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combo.Ready"), false);
        const bool bHasReadyTag = (ASC && ASC->HasMatchingGameplayTag(ReadyTag));

        // 서버 및 클라이언트 모두 사전 차단 (유효한 콤보가 아니면 사용 불가)
        if (!bHasValidCombo && !bHasReadyTag)
        {
            UE_LOG(LogTemp, Warning, TEXT("[SkillManager] Skill %s is Combo Only! Standalone activation blocked."), *SkillId.ToString());
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red, FString::Printf(TEXT("⚠️ [%s] 연계 콤보 상태에서만 사용 가능!"), *Row->DisplayName.ToString()));
            return false;
        }
    }

    // 4. 🔥 [New] 분노 전용 스킬 조건 검사 (bRequiresRageState == true)
    if (Row->Combo.bRequiresRageState)
    {
        static const FGameplayTag RageStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
        if (!ASC->HasMatchingGameplayTag(RageStateTag))
        {
            UE_LOG(LogTemp, Warning, TEXT("[SkillManager] '%s' 스킬은 분노 상태(State.Rage)에서만 시전 가능합니다!"), *SkillId.ToString());
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red, FString::Printf(TEXT("⚠️ [%s] 분노 상태에서만 사용 가능!"), *Row->DisplayName.ToString()));
            return false;
        }
    }

    // 5. 🔥 자원 코스트(SP / MP / HP) 부족 검사
    const float CostVal = GetSkillCost(*Row, Level);
    if (CostVal > 0.f)
    {
        FGameplayAttribute CostAttr;
        FString CostName = TEXT("자원");
        if (Row->Cost.CostType == ESkillCostType::SP) { CostAttr = UNonAttributeSet::GetSPAttribute(); CostName = TEXT("SP"); }
        else if (Row->Cost.CostType == ESkillCostType::MP) { CostAttr = UNonAttributeSet::GetMPAttribute(); CostName = TEXT("MP"); }
        else if (Row->Cost.CostType == ESkillCostType::HP) { CostAttr = UNonAttributeSet::GetHPAttribute(); CostName = TEXT("HP"); }

        if (CostAttr.IsValid())
        {
            const float CurrentVal = ASC->GetNumericAttribute(CostAttr);
            if (CurrentVal < CostVal)
            {
                UE_LOG(LogTemp, Warning, TEXT("⚠️ [SkillManager] '%s' %s 부족! (필요: %.0f / 현재: %.0f)"), *SkillId.ToString(), *CostName, CostVal, CurrentVal);
                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red, FString::Printf(TEXT("⚠️ %s가 부족합니다! (필요: %.0f)"), *CostName, CostVal));
                return false;
            }
        }
    }

    // GA_SkillBase 에서 어떤 스킬인지 알 수 있도록 미리 저장
    PendingSkillId = SkillId;

    // ── ⚡ [Multiplayer Combo Fix] 연계 콤보 스킬에 한해 1타 시전 중 2타 발동 시 캔슬 허용! ──
    // 단, 평온(Tranquility) 같은 단발 스킬은 이미 시전 중(Active)이라면 연타에 의한 중복 발동/캔슬 원천 차단!
    const bool bIsComboSkill = (!Row->Combo.NextComboSkillId.IsNone() || Row->Combo.bIsComboOnlySkill);
    if (Row->Combat.AbilityClass)
    {
        for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
        {
            if (Spec.Ability && Spec.Ability->GetClass() == Row->Combat.AbilityClass && Spec.IsActive())
            {
                if (bIsComboSkill)
                {
                    ASC->CancelAbility(Spec.Ability);
                }
                else
                {
                    // 단발 스킬은 이미 시전 중이면 연타 중복 시전 방지
                    UE_LOG(LogTemp, Warning, TEXT("[SkillManager] '%s' 스킬은 이미 시전 중(Active)이므로 중복 발동을 차단합니다."), *SkillId.ToString());
                    PendingSkillId = NAME_None;
                    return false;
                }
            }
        }
    }

    // GA 발동 시도 (GA_SkillBase 의 Activation Blocked Tags 가 State.Skill 차단을 100% 자동 전담)
    const bool bActivated = ASC->TryActivateAbilityByClass(Row->Combat.AbilityClass);
    if (!bActivated)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ [SkillManager] ASC->TryActivateAbilityByClass('%s') 실패! (어빌리티 클래스 부여 여부 확인 필요)"), *GetNameSafe(Row->Combat.AbilityClass));
        // 실패했으면 Pending 초기화
        PendingSkillId = NAME_None;
        return false;
    }

    // [New] 연계 스킬로 정상 발동되었으면 이전 부모 스킬의 콤보 창을 안전하게 청소
    TArray<FName> ExpiredParents;
    for (const auto& ChainPair : ActiveComboChains)
    {
        if (ChainPair.Value == SkillId)
        {
            ExpiredParents.Add(ChainPair.Key);
        }
    }
    for (const FName& ParentId : ExpiredParents)
    {
        ClearComboReadyTag(ParentId);
    }

    // ── ⏳ [AnimNotify 연동] 연계 스킬 정보 대기 등록 (ANS_ComboWindow 노티파이가 열어줄 때까지 보관) ──
    if (!Row->Combo.NextComboSkillId.IsNone() && Row->Combo.ComboWindowDuration > 0.f && GetSkillLevel(Row->Combo.NextComboSkillId) > 0)
    {
        PendingComboBaseSkillId = SkillId;
        PendingComboNextSkillId = Row->Combo.NextComboSkillId;
        PendingComboDuration = Row->Combo.ComboWindowDuration;
    }
    else
    {
        PendingComboBaseSkillId = NAME_None;
        PendingComboNextSkillId = NAME_None;
        PendingComboDuration = 0.f;
    }

    // === 쿨타임 시작 ===
    // (GA 내부 Commit으로 처리하는 게 정석이지만, 현재 구조상 매니저가 관리)
    const float CdBase = Row->Cost.Cooldown;
    const float CdPerLevel = Row->Cost.CooldownPerLevel;
    float Duration = CdBase + CdPerLevel * FMath::Max(0, Level - 1);

    if (Duration > 0.f)
    {
        // ── [추가] 쿨타임 감소(CDR) 스탯 연동 ──
        // (예: 아이템이나 패시브로 얻은 CooldownReduction 값이 20이면 20% 감소)
        float CDReduction = ASC->GetNumericAttribute(UNonAttributeSet::GetCooldownReductionAttribute());
        CDReduction = FMath::Clamp(CDReduction, 0.f, 90.f); // 밸런스 붕괴 방지: 쿨감 최대 90%로 제한
        Duration = Duration * (1.0f - (CDReduction / 100.f));

        if (UWorld* World = GetWorld())
        {
            const float Now = World->GetTimeSeconds();
            const float EndTime = Now + Duration;
            CooldownEndTimes.Add(SkillId, EndTime);
            //퀵슬롯에 알려줌 (서버->클라 RPC 없음. 각자 돔)
            OnSkillCooldownStarted.Broadcast(SkillId, Duration, EndTime);
        }
    }
    return true;
}

void USkillManagerComponent::ApplyActive_GiveOrUpdate(const FSkillRow& Row, int32 NewLevel)
{
    if (!ASC || !Row.Combat.AbilityClass) return;

    FGameplayAbilitySpec* Found = nullptr;
    for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
    {
        if (Spec.Ability && Spec.Ability->GetClass() == Row.Combat.AbilityClass)
        {
            Found = &Spec; break;
        }
    }

    if (Found)
    {
        Found->Level = NewLevel;
        ASC->MarkAbilitySpecDirty(*Found);
    }
    else
    {
        FGameplayAbilitySpec Spec(Row.Combat.AbilityClass, NewLevel, INDEX_NONE, this);
        ASC->GiveAbility(Spec);
    }
}

void USkillManagerComponent::ApplyPassive_ApplyOrStack(FName SkillId, const FSkillRow& Row, int32 NewLevel)
{
    // [New] 패시브 스킬이 상태이상을 유발하는 경우 캐릭터 캐시 필드에 스탯 및 정보 등록
    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(GetOwner()))
    {
        if (Row.StatusEffect.bHasStatusEffect)
        {
            float Duration = Row.StatusEffect.StatusEffectDurations.IsValidIndex(NewLevel - 1) ? Row.StatusEffect.StatusEffectDurations[NewLevel - 1] : 0.f;
            float Chance = Row.StatusEffect.StatusEffectChances.IsValidIndex(NewLevel - 1) ? Row.StatusEffect.StatusEffectChances[NewLevel - 1] : 0.f;
            float Value = Row.StatusEffect.StatusEffectValues.IsValidIndex(NewLevel - 1) ? Row.StatusEffect.StatusEffectValues[NewLevel - 1] : 0.f;

            NonChar->SetLastSkillStatusEffectDuration(Duration);
            NonChar->SetLastSkillStatusEffectChance(Chance);
            NonChar->SetLastSkillStatusEffectValue(Value);
            NonChar->SetLastSkillLevel(NewLevel);
        }
    }

    if (!ASC || !Row.Passive.PassiveEffect) return;

    // [New] 적에게 상태이상을 유발하는 패시브의 경우, 플레이어 자신에게는 이펙트(GE_Bleeding 등)를 부여하지 않음
    if (Row.StatusEffect.bHasStatusEffect) return;

    // 기존에 적용된 이전 레벨의 패시브 GE가 있다면 깔끔히 제거 (중복 누적 및 수치 미갱신 방지)
    if (FActiveGameplayEffectHandle* FoundHandle = ActivePassiveHandles.Find(SkillId))
    {
        if (FoundHandle->IsValid())
        {
            ASC->RemoveActiveGameplayEffect(*FoundHandle);
        }
        ActivePassiveHandles.Remove(SkillId);
    }

    FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
    Ctx.AddInstigator(GetOwner(), GetOwner());
    FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(Row.Passive.PassiveEffect, 1.f, Ctx);
    if (!SpecHandle.IsValid()) return;

    // ① 스택식: 레벨=스택 (원하면 이 방식 사용)
    // SpecHandle.Data->SetStackCount(NewLevel);

    // ② SetByCaller식: 레벨별 수치 (PassiveValues 우선, 없으면 LevelScalars, 없으면 NewLevel)
    float Value = NewLevel;
    if (Row.Passive.PassiveValues.IsValidIndex(NewLevel - 1))
    {
        Value = Row.Passive.PassiveValues[NewLevel - 1];
    }
    else if (Row.Combat.LevelScalars.IsValidIndex(NewLevel - 1))
    {
        Value = Row.Combat.LevelScalars[NewLevel - 1];
    }

    if (!Row.Passive.SetByCallerKey.IsNone())
    {
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Row.Passive.SetByCallerKey, false);
        if (Tag.IsValid())
        {
            SpecHandle.Data->SetSetByCallerMagnitude(Tag, Value);
        }
        SpecHandle.Data->SetSetByCallerMagnitude(Row.Passive.SetByCallerKey, Value);
    }

    FActiveGameplayEffectHandle AppliedHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    if (AppliedHandle.IsValid())
    {
        ActivePassiveHandles.Add(SkillId, AppliedHandle);
    }
}

float USkillManagerComponent::GetSkillCost(const FSkillRow& Row, int32 Level) const
{
    const int32 L = FMath::Max(1, Level);
    return Row.Cost.CostValue + Row.Cost.CostValuePerLevel * (L - 1);
}

bool USkillManagerComponent::GetSkillCooldownDetails(FName SkillId, float& OutRemaining, float& OutTotalDuration) const
{
    OutRemaining = 0.f;
    OutTotalDuration = 0.f;

    if (!IsOnCooldown(SkillId, OutRemaining))
    {
        return false;
    }

    if (DataAsset)
    {
        if (const FSkillRow* Row = DataAsset->Skills.Find(SkillId))
        {
            const int32 Lv = GetSkillLevel(SkillId);
            const float CdBase = Row->Cost.Cooldown;
            const float CdPerLevel = Row->Cost.CooldownPerLevel;
            OutTotalDuration = CdBase + CdPerLevel * FMath::Max(0, Lv - 1);
            OutTotalDuration = FMath::Max(OutTotalDuration, OutRemaining);
            return true;
        }
    }

    OutTotalDuration = OutRemaining;
    return true;
}

bool USkillManagerComponent::CanLearnSkill(FName SkillId, int32& OutRequiredCharLevel) const
{
    OutRequiredCharLevel = 1;
    if (!DataAsset) return false;

    const FSkillRow* Row = DataAsset->Skills.Find(SkillId);
    if (!Row) return false;

    const int32 CurrentLv = GetSkillLevel(SkillId);
    if (CurrentLv >= Row->MaxLevel) return false; // 이미 만렙

    if (SkillPoints < 1) return false; // 스킬포인트 부족

    const int32 NextLv = CurrentLv + 1;
    const int32 NextLvIdx = NextLv - 1;

    OutRequiredCharLevel = Row->RequiredCharacterLevel;
    if (Row->RequiredCharacterLevelPerLevel.IsValidIndex(NextLvIdx))
    {
        OutRequiredCharLevel = Row->RequiredCharacterLevelPerLevel[NextLvIdx];
    }

    // 현재 캐릭터 레벨 구하기
    int32 CurrentCharLevel = 1;
    if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(GetOwner()))
    {
        if (const UNonAttributeSet* AS = Char->GetAttributeSet())
        {
            CurrentCharLevel = FMath::RoundToInt(AS->GetLevel());
        }
    }

    return (CurrentCharLevel >= OutRequiredCharLevel);
}

float USkillManagerComponent::GetDurationBonusForTag(const FGameplayTag& Tag) const
{
    if (!Tag.IsValid() || !DataAsset)
    {
        return 0.f;
    }

    float TotalBonus = 0.f;

    // 플레이어가 습득한 모든 스킬 중 패시브 스킬 탐색 (데이터 주도 범용 지속시간 시스템)
    for (const auto& Pair : DataAsset->Skills)
    {
        const FSkillRow& Row = Pair.Value;
        if (Row.Type == ESkillType::Passive && Row.Passive.TargetDurationTag == Tag)
        {
            const int32 Lv = GetSkillLevel(Row.Id);
            if (Lv > 0)
            {
                if (Row.Passive.PassiveValues.IsValidIndex(Lv - 1))
                {
                    TotalBonus += Row.Passive.PassiveValues[Lv - 1];
                }
            }
        }
    }

    return TotalBonus;
}

float USkillManagerComponent::GetRageDurationBonus() const
{
    static const FGameplayTag RageStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
    float Bonus = GetDurationBonusForTag(RageStateTag);

    // [Fallback] 혹시 DA에서 TargetDurationTag 설정을 깜빡했거나 스킬 ID(B_P_RageDuration 등)로 직접 세팅된 경우 대비
    if (Bonus <= 0.f)
    {
        const FName PossibleIds[] = { FName("B_P_RageDuration"), FName("B_P_PersistentRage"), FName("B_P_EndlessRage") };
        for (const FName& CheckId : PossibleIds)
        {
            const int32 Lv = GetSkillLevel(CheckId);
            if (Lv > 0)
            {
                if (DataAsset)
                {
                    if (const FSkillRow* Row = DataAsset->Skills.Find(CheckId))
                    {
                        if (Row->Passive.PassiveValues.IsValidIndex(Lv - 1))
                        {
                            return Row->Passive.PassiveValues[Lv - 1];
                        }
                    }
                }
                return static_cast<float>(Lv * 5.0f); // 기본 fallback (레벨당 5초)
            }
        }
    }

    return Bonus;
}

float USkillManagerComponent::GetBaseRageDuration() const
{
    // 🌟 DA_Skill_Berserker -> B_P_Rage (분노) 행의 PassiveValues[0]에 값이 있으면 최우선 적용!
    if (DataAsset)
    {
        if (const FSkillRow* Row = DataAsset->Skills.Find(FName("B_P_Rage")))
        {
            if (Row->Passive.PassiveValues.IsValidIndex(0) && Row->Passive.PassiveValues[0] > 0.f)
            {
                return Row->Passive.PassiveValues[0];
            }
        }
    }
    return 15.0f; // 기본 15초 fallback
}

float USkillManagerComponent::GetTotalRageDuration(float BaseDuration) const
{
    const float Base = (BaseDuration > 0.f) ? BaseDuration : GetBaseRageDuration();
    return Base + GetRageDurationBonus();
}

int32 USkillManagerComponent::GetRequiredRageHitCount() const
{
    const int32 MasteryLv = GetSkillLevel(FName("B_P_RageMastery"));

    // 🌟 에디터의 DA_Skill_Berserker -> B_P_RageMastery 행에 PassiveValues가 등록되어 있다면 그 값을 최우선 사용!
    if (DataAsset && MasteryLv > 0)
    {
        if (const FSkillRow* Row = DataAsset->Skills.Find(FName("B_P_RageMastery")))
        {
            if (Row->Passive.PassiveValues.IsValidIndex(MasteryLv - 1))
            {
                return FMath::RoundToInt(Row->Passive.PassiveValues[MasteryLv - 1]);
            }
        }
    }

    // 기본 단계별 타격수: 0레벨 8회, 1레벨 7회, 2레벨 6회, 3레벨 이상 5회
    if (MasteryLv >= 3) return 5;
    if (MasteryLv == 2) return 6;
    if (MasteryLv == 1) return 7;
    return 8; // 기본 8회
}

bool USkillManagerComponent::AddRageHitCount(int32 Delta)
{
    // 🔥 분노 상태가 이미 활성화 중이면 지속시간 동안 추가 카운팅 방지!
    if (bIsRageActive)
    {
        return false;
    }

    CurrentRageHitCount += Delta;
    const int32 TargetCount = GetRequiredRageHitCount();

    UE_LOG(LogTemp, Warning, TEXT("[Rage System Debug] Hit Count: %d / %d (Required: %d)"), CurrentRageHitCount, TargetCount, TargetCount);

    if (CurrentRageHitCount >= TargetCount)
    {
        CurrentRageHitCount = 0;
        // 🔥 타격 카운트 달성으로 터지는 분노: 패시브가 적용된 분노 총 지속시간(기본15초+보너스)으로 발동!
        ActivateRageState();
        return true;
    }
    return false;
}

void USkillManagerComponent::ActivateRageState(float Duration)
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    bIsRageActive = true;

    // 🔥 Duration이 지정되지 않았거나 음수면, 패시브가 적용된 분노 총 지속시간(기본 15초 + 패시브 5/10초)으로 자동 계산!
    const float FinalDuration = (Duration > 0.f) ? Duration : GetTotalRageDuration();

    UAbilitySystemComponent* LocalASC = ASC ? ASC.Get() : OwnerActor->FindComponentByClass<UAbilitySystemComponent>();
    if (LocalASC)
    {
        static const FGameplayTag RageStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
        
        // 🔥 SkillManagerComponent 전용 LooseTag 부여 보장 (GE_Berserk가 먼저 꺼져도 분노 태그 보존)
        if (!bHasRageLooseTag)
        {
            LocalASC->AddLooseGameplayTag(RageStateTag);
            bHasRageLooseTag = true;
        }

        FTimerDelegate RageTimerDel;
        RageTimerDel.BindUObject(this, &USkillManagerComponent::DeactivateRageState);
        
        if (UWorld* World = GetWorld())
        {
            // 🔥 이미 분노가 켜져 있었더라도 최종 시간(FinalDuration)으로 타이머를 다시 처음부터 새로고침!
            World->GetTimerManager().SetTimer(RageTimerHandle, RageTimerDel, FinalDuration, false);
        }

        UE_LOG(LogTemp, Warning, TEXT("[Rage System Debug] RAGE STATE ACTIVATED/REFRESHED! (State.Rage Granted for %.1fs, Crit +10%%)"), FinalDuration);
    }

    // 클라이언트 UI 및 ASC에도 즉시 분노 상태 전파 (HUD 타이머 리셋)
    if (OwnerActor->HasAuthority())
    {
        Client_ActivateRageState(FinalDuration);
    }
}

void USkillManagerComponent::DeactivateRageState()
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    bIsRageActive = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RageTimerHandle);
    }

    UAbilitySystemComponent* LocalASC = ASC ? ASC.Get() : OwnerActor->FindComponentByClass<UAbilitySystemComponent>();
    if (LocalASC)
    {
        static const FGameplayTag RageStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
        if (bHasRageLooseTag)
        {
            LocalASC->RemoveLooseGameplayTag(RageStateTag);
            bHasRageLooseTag = false;
        }

        // 🔥 State.Rage 태그를 부여하고 있는 모든 액티브 버프(예: GE_Berserk 등)를 즉시 강제 제거!
        FGameplayTagContainer RageContainer;
        RageContainer.AddTag(RageStateTag);
        LocalASC->RemoveActiveEffectsWithGrantedTags(RageContainer);

        UE_LOG(LogTemp, Warning, TEXT("[Rage System Debug] RAGE STATE EXPIRED! (State.Rage & Active GE Buffs Removed)"));
    }

    // 클라이언트 UI 및 ASC에도 즉시 분노 해제 전파
    if (OwnerActor->HasAuthority())
    {
        Client_DeactivateRageState();
    }
}

void USkillManagerComponent::Client_ActivateRageState_Implementation(float Duration)
{
    bIsRageActive = true;
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    UAbilitySystemComponent* LocalASC = ASC ? ASC.Get() : OwnerActor->FindComponentByClass<UAbilitySystemComponent>();
    if (LocalASC)
    {
        static const FGameplayTag RageStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
        if (!LocalASC->HasMatchingGameplayTag(RageStateTag))
        {
            LocalASC->AddLooseGameplayTag(RageStateTag);
        }
    }

    // 🔥 HUD 버프 바에 분노 이름, 아이콘 및 15초 남은 시간 실시간 등록!
    UTexture2D* RageIcon = nullptr;
    FText RageName = FText::FromString(TEXT("분노"));
    if (DataAsset)
    {
        if (const FSkillRow* Row = DataAsset->Skills.Find(FName("B_P_Rage")))
        {
            RageIcon = Row->Icon.LoadSynchronous();
            if (!Row->DisplayName.IsEmpty())
            {
                RageName = Row->DisplayName;
            }
        }
    }

    if (UNonUIManagerComponent* UIMgr = OwnerActor->FindComponentByClass<UNonUIManagerComponent>())
    {
        if (UInGameHUD* HUD = UIMgr->GetInGameHUD())
        {
            HUD->AddOrUpdateBuff(FName("State.Rage"), RageName, RageIcon, Duration);
        }
    }
}

void USkillManagerComponent::Client_DeactivateRageState_Implementation()
{
    bIsRageActive = false;
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    UAbilitySystemComponent* LocalASC = ASC ? ASC.Get() : OwnerActor->FindComponentByClass<UAbilitySystemComponent>();
    if (LocalASC)
    {
        static const FGameplayTag RageStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
        LocalASC->RemoveLooseGameplayTag(RageStateTag);

        FGameplayTagContainer RageContainer;
        RageContainer.AddTag(RageStateTag);
        LocalASC->RemoveActiveEffectsWithGrantedTags(RageContainer);
    }

    // 🔥 HUD 버프 바에서 분노 아이콘 및 관련 버프 제거
    if (UNonUIManagerComponent* UIMgr = OwnerActor->FindComponentByClass<UNonUIManagerComponent>())
    {
        if (UInGameHUD* HUD = UIMgr->GetInGameHUD())
        {
            HUD->RemoveBuff(FName("State.Rage"));
            HUD->RemoveBuff(FName("B_A_Berserk"));
            HUD->RemoveBuff(FName("Berserk"));
        }
    }
}

void USkillManagerComponent::OnRep_IsRageActive()
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    UAbilitySystemComponent* LocalASC = ASC ? ASC.Get() : OwnerActor->FindComponentByClass<UAbilitySystemComponent>();
    if (LocalASC)
    {
        static const FGameplayTag RageStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
        if (bIsRageActive)
        {
            if (!LocalASC->HasMatchingGameplayTag(RageStateTag))
            {
                LocalASC->AddLooseGameplayTag(RageStateTag);
            }
        }
        else
        {
            LocalASC->RemoveLooseGameplayTag(RageStateTag);
        }
    }
}

void USkillManagerComponent::OnRageTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    if (NewCount > 0)
    {
        bIsRageActive = true;
    }
    else
    {
        // 태그 카운트가 0이 되었을 때: 만약 분노 타이머가 아직 살아있다면 태그를 즉시 복원!
        if (bIsRageActive)
        {
            if (UWorld* World = GetWorld())
            {
                if (World->GetTimerManager().IsTimerActive(RageTimerHandle))
                {
                    if (UAbilitySystemComponent* LocalASC = ASC ? ASC.Get() : OwnerActor->FindComponentByClass<UAbilitySystemComponent>())
                    {
                        static const FGameplayTag RageStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
                        LocalASC->AddLooseGameplayTag(RageStateTag);
                        bHasRageLooseTag = true;
                        return; // 삭제 중단 (분노 지속시간 유지!)
                    }
                }
            }
        }

        bIsRageActive = false;
        bHasRageLooseTag = false;
        if (UNonUIManagerComponent* UIMgr = OwnerActor->FindComponentByClass<UNonUIManagerComponent>())
        {
            if (UInGameHUD* HUD = UIMgr->GetInGameHUD())
            {
                HUD->RemoveBuff(FName("State.Rage"));
            }
        }
    }
}

TMap<FName, int32> USkillManagerComponent::GetSkillLevelMap() const
{
    TMap<FName, int32> Result;
    for (const FSkillLevelEntry& E : SkillLevels.Items)
    {
        if (E.Level > 0)
        {
            Result.Add(E.SkillId, E.Level);
        }
    }
    return Result;
}

void USkillManagerComponent::RestoreSkillLevels(const TMap<FName, int32>& InMap)
{
    // 기존 스킬 초기화 필요 시 수행 (지금은 덮어쓰기)
    
    if (!DataAsset) return;

    for (const auto& Elem : InMap)
    {
        const FName Id = Elem.Key;
        const int32 Lv = Elem.Value;

        if (const FSkillRow* Row = DataAsset->Skills.Find(Id))
        {
            // 레벨 설정
            SkillLevels.SetLevel(Id, Lv);
            
            // GA/GE 재적용
            if (Row->Type == ESkillType::Active)  ApplyActive_GiveOrUpdate(*Row, Lv);
            else                                   ApplyPassive_ApplyOrStack(Id, *Row, Lv);

            // UI 알림
            OnSkillLevelChanged.Broadcast(Id, Lv);
        }
    }
    
    // 포인트 변경 알림
    OnSkillPointsChanged.Broadcast(SkillPoints);
}

FName USkillManagerComponent::GetActiveComboSkillId(FName BaseSkillId) const
{
    if (const FName* NextComboId = ActiveComboChains.Find(BaseSkillId))
    {
        return *NextComboId;
    }
    return BaseSkillId;
}

void USkillManagerComponent::ClearComboReadyTag(FName BaseSkillId)
{
    if (!ASC) return;

    FString TagString = FString::Printf(TEXT("State.Combo.Ready.%s"), *BaseSkillId.ToString());
    FGameplayTag ComboTag = FGameplayTag::RequestGameplayTag(*TagString, false);
    
    FGameplayTag ReadyTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combo.Ready"), false);

    if (ComboTag.IsValid())
    {
        ASC->RemoveLooseGameplayTag(ComboTag);
    }
    
    FGameplayTagContainer ReadyContainer;
    ReadyContainer.AddTag(ReadyTag);
    
    // 만약 다른 연계 대기 태그가 없다면 최상위 Ready 태그 제거
    bool bHasOtherCombo = false;
    for (const auto& Elem : ComboWindowTimerHandles)
    {
        if (Elem.Key != BaseSkillId)
        {
            bHasOtherCombo = true;
            break;
        }
    }

    if (!bHasOtherCombo)
    {
        ASC->RemoveLooseGameplayTag(ReadyTag);
    }

    if (FTimerHandle* HandlePtr = ComboWindowTimerHandles.Find(BaseSkillId))
    {
        GetWorld()->GetTimerManager().ClearTimer(*HandlePtr);
        ComboWindowTimerHandles.Remove(BaseSkillId);
    }

    // [New] 서버 로컬 콤보 맵에서 해당 연계 체인 정보를 확실히 제거합니다.
    ActiveComboChains.Remove(BaseSkillId);

    OnComboWindowChanged.Broadcast(BaseSkillId, NAME_None, 0.0f, 0.0f, 0.0f);

    // [New] 서버에서 콤보가 종료(만료)되었으므로 클라이언트 상태를 동기화하여 연계 대기창을 닫아줍니다.
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ClientSyncComboState(BaseSkillId, NAME_None, 0.0f, 0.0f, 0.0f);
    }
}

void USkillManagerComponent::OpenPendingComboWindow()
{
    if (PendingComboBaseSkillId.IsNone() || PendingComboNextSkillId.IsNone() || PendingComboDuration <= 0.f)
    {
        return;
    }

    const FName SkillId = PendingComboBaseSkillId;
    const FName NextSkillId = PendingComboNextSkillId;
    const float Duration = PendingComboDuration;

    PendingComboBaseSkillId = NAME_None;
    PendingComboNextSkillId = NAME_None;
    PendingComboDuration = 0.f;

    if (!ASC) return;

    if (FTimerHandle* ExistingHandle = ComboWindowTimerHandles.Find(SkillId))
    {
        GetWorld()->GetTimerManager().ClearTimer(*ExistingHandle);
    }

    FGameplayTag ReadyTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combo.Ready"), false);
    ASC->AddLooseGameplayTag(ReadyTag);

    FString TagString = FString::Printf(TEXT("State.Combo.Ready.%s"), *SkillId.ToString());
    FGameplayTag ComboTag = FGameplayTag::RequestGameplayTag(*TagString, false);
    if (ComboTag.IsValid())
    {
        ASC->AddLooseGameplayTag(ComboTag);
    }

    // 로컬 콤보 맵에 연계 정보 등록 (서버 및 클라이언트 양쪽 즉시 캐싱)
    ActiveComboChains.Add(SkillId, NextSkillId);

    FTimerHandle& ComboTimer = ComboWindowTimerHandles.FindOrAdd(SkillId);
    GetWorld()->GetTimerManager().SetTimer(ComboTimer, [this, SkillId]() {
        OnComboTimerExpired(SkillId);
    }, Duration, false);

    // 다음 연계 스킬 쿨타임 정보 역산
    float CooldownRemaining = 0.f;
    float CooldownTotal = 0.f;
    if (IsOnCooldown(NextSkillId, CooldownRemaining))
    {
        if (DataAsset)
        {
            if (const FSkillRow* NextRow = DataAsset->Skills.Find(NextSkillId))
            {
                const int32 Lv = GetSkillLevel(NextSkillId);
                const float CdBase = NextRow->Cost.Cooldown;
                const float CdPerLevel = NextRow->Cost.CooldownPerLevel;
                CooldownTotal = CdBase + CdPerLevel * FMath::Max(0, Lv - 1);
                CooldownTotal = FMath::Max(CooldownTotal, CooldownRemaining);
            }
        }
    }

    OnComboWindowChanged.Broadcast(SkillId, NextSkillId, Duration, CooldownRemaining, CooldownTotal);

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ClientSyncComboState(SkillId, NextSkillId, Duration, CooldownRemaining, CooldownTotal);
    }
}

void USkillManagerComponent::ClosePendingComboWindow()
{
    PendingComboBaseSkillId = NAME_None;
    PendingComboNextSkillId = NAME_None;
    PendingComboDuration = 0.f;
}

bool USkillManagerComponent::GetComboWindowRemaining(FName BaseSkillId, float& OutRemaining, float& OutDuration) const
{
    OutRemaining = 0.f;
    OutDuration = 0.f;

    if (const FTimerHandle* HandlePtr = ComboWindowTimerHandles.Find(BaseSkillId))
    {
        if (GetWorld())
        {
            OutRemaining = GetWorld()->GetTimerManager().GetTimerRemaining(*HandlePtr);
            OutDuration = GetWorld()->GetTimerManager().GetTimerRate(*HandlePtr);
            return (OutRemaining > 0.f);
        }
    }

    return false;
}

void USkillManagerComponent::OnComboTimerExpired(FName BaseSkillId)
{
    ClearComboReadyTag(BaseSkillId);
    

}

void USkillManagerComponent::ForceClearAllCombos()
{
    if (!ASC) return;

    for (auto& Elem : ComboWindowTimerHandles)
    {
        GetWorld()->GetTimerManager().ClearTimer(Elem.Value);
        
        FString TagString = FString::Printf(TEXT("State.Combo.Ready.%s"), *Elem.Key.ToString());
        FGameplayTag ComboTag = FGameplayTag::RequestGameplayTag(*TagString, false);
        if (ComboTag.IsValid())
        {
            ASC->RemoveLooseGameplayTag(ComboTag);
        }

        // [New] 서버 로컬 콤보 맵에서 해당 체인 정보를 제거합니다.
        ActiveComboChains.Remove(Elem.Key);

        OnComboWindowChanged.Broadcast(Elem.Key, NAME_None, 0.0f, 0.0f, 0.0f);

        // [New] 피격 상태이상 등으로 콤보 강제 제거 시 로컬 클라이언트에도 태그 제거 및 팝업 종료를 강제 지시합니다.
        if (GetOwner() && GetOwner()->HasAuthority())
        {
            ClientSyncComboState(Elem.Key, NAME_None, 0.0f, 0.0f, 0.0f);
        }
    }
    ComboWindowTimerHandles.Empty();
    ActiveComboChains.Empty(); // [New] 전체 콤보 맵을 완벽하게 초기화합니다.

    FGameplayTag ReadyTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combo.Ready"), false);
    ASC->RemoveLooseGameplayTag(ReadyTag);
}

void USkillManagerComponent::ClientSyncComboState_Implementation(FName BaseSkillId, FName NextComboSkillId, float Duration, float CooldownRemaining, float CooldownTotal)
{
    FGameplayTag ReadyTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combo.Ready"), false);
    FString TagString = FString::Printf(TEXT("State.Combo.Ready.%s"), *BaseSkillId.ToString());
    FGameplayTag ComboTag = FGameplayTag::RequestGameplayTag(*TagString, false);

    // [New] 다음 연계 스킬을 아직 학습하지 않았거나 레벨이 0이라면 연계 대기 상태를 적용하지 않고 즉시 차단/만료시킵니다!
    if (NextComboSkillId.IsNone() || Duration <= 0.f || GetSkillLevel(NextComboSkillId) <= 0)
    {
        // [New] 클라이언트 로컬 콤보 맵에서 정보 제거
        ActiveComboChains.Remove(BaseSkillId);

        // 콤보 만료 시 태그 제거
        if (ASC)
        {
            ASC->RemoveLooseGameplayTag(ReadyTag);
            if (ComboTag.IsValid())
            {
                ASC->RemoveLooseGameplayTag(ComboTag);
            }
        }
        
        // 로컬 클라이언트에서 델리게이트 방송을 수행하여 실시간으로 UI/퀵슬롯 아이콘을 업데이트하도록 지시합니다!
        OnComboWindowChanged.Broadcast(BaseSkillId, NAME_None, 0.f, 0.f, 0.f);
    }
    else
    {
        // [New] 클라이언트 로컬 콤보 맵에 정보 등록 (이것이 태그 에러를 우회하는 100% 보장된 핵심 로직입니다!)
        ActiveComboChains.Add(BaseSkillId, NextComboSkillId);

        // 콤보 시작 시 태그 강제 주입
        if (ASC)
        {
            ASC->AddLooseGameplayTag(ReadyTag);
            if (ComboTag.IsValid())
            {
                ASC->AddLooseGameplayTag(ComboTag);
            }
        }

        // 로컬 클라이언트에서 델리게이트 방송을 수행하여 실시간으로 UI/퀵슬롯 아이콘을 업데이트하도록 지시합니다! (전달받은 실시간 쿨타임 정보 릴레이)
        OnComboWindowChanged.Broadcast(BaseSkillId, NextComboSkillId, Duration, CooldownRemaining, CooldownTotal);
    }
}

