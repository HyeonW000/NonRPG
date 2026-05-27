#include "Skill/SkillManagerComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "Ability/NonAttributeSet.h"

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
            if (!Row->PrerequisiteSkillId.IsNone())
            {
                const int32 PreLevel = SkillLevels.GetLevel(Row->PrerequisiteSkillId);
                if (PreLevel < Row->PrerequisiteSkillLevel)
                {
                    OutWhy = FString::Printf(TEXT("Need %s Lv.%d"), *Row->PrerequisiteSkillId.ToString(), Row->PrerequisiteSkillLevel);
                    return false;
                }
            }

            const int32 Cur = SkillLevels.GetLevel(SkillId);
            if (Cur >= Row->MaxLevel) { OutWhy = TEXT("Max level"); }
            else if (SkillPoints <= 0) { OutWhy = TEXT("No Skill Points"); }
            else { OutWhy.Reset(); bResult = true; }
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
    else                                   ApplyPassive_ApplyOrStack(*Row, Cur);

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
        return false;
    }

    float Remaining = 0.f;
    if (IsOnCooldown(SkillId, Remaining))
    {
        return false;
    }
    // 스태미나 체크 (쿨타임 통과 후, AbilityClass 체크 전에)
    {
        const float Cost = GetStaminaCost(*Row, Level);

        if (Cost > 0.f)
        {
            // 실제 현재 SP 가져오기
            const float CurrentSP = ASC->GetNumericAttribute(UNonAttributeSet::GetSPAttribute());

            if (CurrentSP + KINDA_SMALL_NUMBER < Cost)
            {
                return false;
            }
        }
    }

    if (!Row->AbilityClass)
    {
        return false;
    }

    // GA_SkillBase 에서 어떤 스킬인지 알 수 있도록 미리 저장
    PendingSkillId = SkillId;

    // [New] 만약 연계 어빌리티가 시도 중이어서 동일 어빌리티가 이미 활성화 상태라면,
    // 중복 실행 제한에 막히지 않도록 기존 실행 중인 어빌리티 인스턴스를 안전하게 Cancel 시켜 줍니다.
    if (ASC && Row->AbilityClass)
    {
        TArray<FGameplayAbilitySpec>& Specs = ASC->GetActivatableAbilities();
        for (FGameplayAbilitySpec& Spec : Specs)
        {
            if (Spec.Ability && Spec.Ability->GetClass() == Row->AbilityClass)
            {
                if (Spec.IsActive())
                {
                    ASC->CancelAbilityHandle(Spec.Handle);
                    UE_LOG(LogTemp, Log, TEXT("[SkillMgr] 동일 어빌리티(%s)가 이미 실행 중이므로 연계를 위해 기존 어빌리티를 강제로 캔슬합니다."), *Row->AbilityClass->GetName());
                }
            }
        }
    }

    // GA 발동 시도
    const bool bActivated = ASC->TryActivateAbilityByClass(Row->AbilityClass);
    if (!bActivated)
    {
        // 실패했으면 Pending 초기화
        PendingSkillId = NAME_None;
        return false;
    }

    // ── [New] 연계 스킬 시스템 타이머 및 태그 부여 ──
    // 다음 연계 스킬을 스킬창에서 해금(레벨이 0보다 큰 상태)했을 때에만 연계 창(팝업 및 아이콘 변환)을 동적으로 활성화합니다!
    if (!Row->NextComboSkillId.IsNone() && Row->ComboWindowDuration > 0.f && GetSkillLevel(Row->NextComboSkillId) > 0)
    {
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

        FTimerHandle& ComboTimer = ComboWindowTimerHandles.FindOrAdd(SkillId);
        GetWorld()->GetTimerManager().SetTimer(ComboTimer, [this, SkillId]() {
            OnComboTimerExpired(SkillId);
        }, Row->ComboWindowDuration, false);

        // [New] 서버 로컬 콤보 맵에 연계 정보를 즉시 추가하여 콤보 스위칭을 캐싱합니다.
        ActiveComboChains.Add(SkillId, Row->NextComboSkillId);

        // [New] 다음 연계 스킬이 서버에서 현재 쿨타임 중인지 확인하고 쿨타임 정보를 역산합니다.
        float CooldownRemaining = 0.f;
        float CooldownTotal = 0.f;
        if (IsOnCooldown(Row->NextComboSkillId, CooldownRemaining))
        {
            if (const FSkillRow* NextRow = DataAsset->Skills.Find(Row->NextComboSkillId))
            {
                const int32 Lv = GetSkillLevel(Row->NextComboSkillId);
                const float CdBase = NextRow->Cooldown;
                const float CdPerLevel = NextRow->CooldownPerLevel;
                CooldownTotal = CdBase + CdPerLevel * FMath::Max(0, Lv - 1);
                CooldownTotal = FMath::Max(CooldownTotal, CooldownRemaining);
            }
        }

        OnComboWindowChanged.Broadcast(SkillId, Row->NextComboSkillId, Row->ComboWindowDuration, CooldownRemaining, CooldownTotal);

        // [New] 서버가 연계 대기창을 연 시점에 로컬 클라이언트에도 태그와 팝업 델리게이트를 동기화 시킵니다. (서버 측 실시간 쿨타임 정보 포함)
        if (GetOwner() && GetOwner()->HasAuthority())
        {
            ClientSyncComboState(SkillId, Row->NextComboSkillId, Row->ComboWindowDuration, CooldownRemaining, CooldownTotal);
        }

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, FString::Printf(TEXT("%s 스킬 연계 가능! 남은 시간: %.1f초"), *Row->NextComboSkillId.ToString(), Row->ComboWindowDuration));
        }
    }

    // === 쿨타임 시작 ===
    // (GA 내부 Commit으로 처리하는 게 정석이지만, 현재 구조상 매니저가 관리)
    const float CdBase = Row->Cooldown;
    const float CdPerLevel = Row->CooldownPerLevel;
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
    if (!ASC || !Row.AbilityClass) return;

    FGameplayAbilitySpec* Found = nullptr;
    for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
    {
        if (Spec.Ability && Spec.Ability->GetClass() == Row.AbilityClass)
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
        FGameplayAbilitySpec Spec(Row.AbilityClass, NewLevel, INDEX_NONE, this);
        ASC->GiveAbility(Spec);
    }
}

void USkillManagerComponent::ApplyPassive_ApplyOrStack(const FSkillRow& Row, int32 NewLevel)
{
    if (!ASC || !Row.PassiveEffect) return;

    FGameplayEffectContextHandle Ctx;
    FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(Row.PassiveEffect, 1.f, Ctx);
    if (!SpecHandle.IsValid()) return;

    // ① 스택식: 레벨=스택 (원하면 이 방식 사용)
    // SpecHandle.Data->SetStackCount(NewLevel);

    // ② SetByCaller식: 레벨별 수치(둘 중 하나 택1)
    float Value = NewLevel;
    if (Row.LevelScalars.IsValidIndex(NewLevel - 1))
    {
        Value = Row.LevelScalars[NewLevel - 1];
    }

    SpecHandle.Data->SetSetByCallerMagnitude(
        FGameplayTag::RequestGameplayTag(Row.SetByCallerKey, false), Value);

    ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

float USkillManagerComponent::GetStaminaCost(const FSkillRow& Row, int32 Level) const
{
    const int32 L = FMath::Max(1, Level);
    return Row.StaminaCost + Row.StaminaCostPerLevel * (L - 1);
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
            else                                   ApplyPassive_ApplyOrStack(*Row, Lv);

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

void USkillManagerComponent::OnComboTimerExpired(FName BaseSkillId)
{
    ClearComboReadyTag(BaseSkillId);
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, FString::Printf(TEXT("%s 스킬 연계 대기 시간이 만료되었습니다."), *BaseSkillId.ToString()));
    }
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
        
        UE_LOG(LogTemp, Log, TEXT("[ClientRPC] 콤보 연계 상태 종료 동기화 완료. (선행 스킬: %s)"), *BaseSkillId.ToString());
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
        
        UE_LOG(LogTemp, Log, TEXT("[ClientRPC] 콤보 연계 상태 활성화 동기화 완료. (선행 스킬: %s, 다음 스킬: %s, 지속시간: %.2f초, 남은 쿨타임: %.2f초)"), 
            *BaseSkillId.ToString(), *NextComboSkillId.ToString(), Duration, CooldownRemaining);
    }
}

