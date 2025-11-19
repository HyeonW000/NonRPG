#include "Skill/SkillManagerComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

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
    SkillLevels.Owner = this; // ★ FastArray 컨테이너에 소유자 연결
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
    // 서버/클라 공용 호출 허용. 권한 체크는 호출측에서 하길 권장.
    if (JobClass != InJob)
    {
        JobClass = InJob;
        OnRep_JobClass(); // 서버에서도 즉시 브로드캐스트
    }
}

void USkillManagerComponent::ServerSetJobClass_Implementation(EJobClass NewJob)
{
    SetJobClass(NewJob);
}

void USkillManagerComponent::Init(EJobClass InClass, USkillDataAsset* InData, UAbilitySystemComponent* InASC)
{
    JobClass = InClass;
    DataAsset = InData;
    ASC = InASC;
    SkillLevels.Owner = this; // 재보장(클라 생성 시)
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

bool USkillManagerComponent::CanLevelUp(FName SkillId, FString& OutWhy) const
{
    if (!DataAsset) { OutWhy = TEXT("No DataAsset"); return false; }
    const FSkillRow* Row = DataAsset->Skills.Find(SkillId);
    if (!Row) { OutWhy = TEXT("No Skill Row"); return false; }

    if (Row->AllowedClass != JobClass) { OutWhy = TEXT("Class mismatch"); return false; }

    const int32 Cur = SkillLevels.GetLevel(SkillId);
    if (Cur >= Row->MaxLevel) { OutWhy = TEXT("Max level"); return false; }

    if (SkillPoints <= 0) { OutWhy = TEXT("No Skill Points"); return false; }

    // 캐릭터 레벨 조건이 필요하면 여기서 체크
    OutWhy.Reset();
    return true;
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

    // 클라: 서버 RPC
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

    // 포인트 차감 + 레벨 증가
    SkillPoints = FMath::Max(0, SkillPoints - 1);
    int32 Cur = SkillLevels.GetLevel(SkillId);
    Cur = FMath::Clamp(Cur + 1, 1, Row->MaxLevel);
    SkillLevels.SetLevel(SkillId, Cur);  // FastArray 수정(자동 Dirty)

    // 적용
    if (Row->Type == ESkillType::Active)  ApplyActive_GiveOrUpdate(*Row, Cur);
    else                                   ApplyPassive_ApplyOrStack(*Row, Cur);

    // 브로드캐스트
    OnSkillPointsChanged.Broadcast(SkillPoints);
    OnSkillLevelChanged.Broadcast(SkillId, Cur);

    // FastArray는 별도 호출 없이 복제됨
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
