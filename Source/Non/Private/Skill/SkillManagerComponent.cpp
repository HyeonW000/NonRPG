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
    OnJobChanged.Broadcast(JobClass);
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

    // 포인트 차감
    SkillPoints = FMath::Max(0, SkillPoints - 1);
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

    // GA 발동 시도
    const bool bActivated = ASC->TryActivateAbilityByClass(Row->AbilityClass);
    if (!bActivated)
    {
        // 실패했으면 Pending 초기화
        PendingSkillId = NAME_None;
        return false;
    }

    // === 쿨타임 시작 ===
    const float CdBase = Row->Cooldown;
    const float CdPerLevel = Row->CooldownPerLevel;
    const float Duration = CdBase + CdPerLevel * FMath::Max(0, Level - 1);

    if (Duration > 0.f)
    {
        if (UWorld* World = GetWorld())
        {
            const float Now = World->GetTimeSeconds();
            const float EndTime = Now + Duration;
            CooldownEndTimes.Add(SkillId, EndTime);
            //퀵슬롯에 알려줌
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
        FGameplayTag::RequestGameplayTag(Row.SetByCallerKey), Value);

    ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

float USkillManagerComponent::GetStaminaCost(const FSkillRow& Row, int32 Level) const
{
    const int32 L = FMath::Max(1, Level);
    return Row.StaminaCost + Row.StaminaCostPerLevel * (L - 1);
}
