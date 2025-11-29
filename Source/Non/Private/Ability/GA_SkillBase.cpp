#include "Ability/GA_SkillBase.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/NonCharacterBase.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"

void UGA_SkillBase::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!ActorInfo || !ActorInfo->OwnerActor.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    AActor* OwnerActor = ActorInfo->OwnerActor.Get();
    if (!OwnerActor)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // SkillManager 찾기
    USkillManagerComponent* SkillMgr =
        OwnerActor->FindComponentByClass<USkillManagerComponent>();

    if (!SkillMgr)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // SkillManager에서 방금 요청한 SkillId 가져오기
    const FName SkillId = SkillMgr->ConsumePendingSkillId();
    if (SkillId.IsNone())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // DataAsset / SkillRow 가져오기
    USkillDataAsset* DA = SkillMgr->GetDataAsset();
    if (!DA)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    const FSkillRow* Row = DA->Skills.Find(SkillId);
    if (!Row)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    const int32 Level = SkillMgr->GetSkillLevel(SkillId);

    // === SP 소모량 계산 ===
    const float StaminaCost = SkillMgr->GetStaminaCost(*Row, Level);
    // === 실제 SP 차감 ===
    if (StaminaCost > 0.f)
    {
        if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
        {
            const FGameplayAttribute SPAttr = UNonAttributeSet::GetSPAttribute();
            const float CurrentSP = ASC->GetNumericAttribute(SPAttr);
            const float NewSP = FMath::Max(0.f, CurrentSP - StaminaCost);
        }
        else
        {
        }
    }

    // 몽타주 재생
    if (Row->Montage)
    {
        UAbilityTask_PlayMontageAndWait* Task =
            UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
                this,
                NAME_None,
                Row->Montage,
                1.f      // PlayRate
            );

        if (Task)
        {
            Task->OnCompleted.AddDynamic(this, &UGA_SkillBase::OnMontageFinished);
            Task->OnCancelled.AddDynamic(this, &UGA_SkillBase::OnMontageCancelled);
            Task->OnInterrupted.AddDynamic(this, &UGA_SkillBase::OnMontageCancelled);

            Task->ReadyForActivation();
            // → 몽타주 끝날 때까지 GA 유지
            return;
        }
    }

    // 몽타주가 없거나 Task 생성 실패 시 그냥 즉시 종료
    EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}

void UGA_SkillBase::OnMontageFinished()
{
    if (IsActive())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
    }
}

void UGA_SkillBase::OnMontageCancelled()
{
    if (IsActive())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}
