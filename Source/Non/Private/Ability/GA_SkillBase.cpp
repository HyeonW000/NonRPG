#include "Ability/GA_SkillBase.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

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
        UE_LOG(LogTemp, Warning, TEXT("[GA_SkillBase] No SkillManagerComponent on %s"),
            *OwnerActor->GetName());

        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // SkillManager에서 방금 요청한 SkillId 가져오기
    const FName SkillId = SkillMgr->ConsumePendingSkillId();
    if (SkillId.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_SkillBase] No PendingSkillId"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // DataAsset / SkillRow 가져오기
    USkillDataAsset* DA = SkillMgr->GetDataAsset();
    if (!DA)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_SkillBase] No DataAsset"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    const FSkillRow* Row = DA->Skills.Find(SkillId);
    if (!Row)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[GA_SkillBase] Row not found for %s in %s"),
            *SkillId.ToString(),
            *GetNameSafe(DA));

        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    const int32 Level = SkillMgr->GetSkillLevel(SkillId);

    UE_LOG(LogTemp, Log,
        TEXT("[GA_SkillBase] Activate SkillId=%s Level=%d Cooldown=%.2f"),
        *SkillId.ToString(),
        Level,
        Row->Cooldown);

    // ==========================
    //   1) 몽타주 재생 (있으면)
    // ==========================
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
