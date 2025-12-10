#include "Ability/Skills/GA_SkillBase.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/NonCharacterBase.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Character/NonCharacterBase.h"

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

    // ASC 필수
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 카메라 방향 정렬 + 풀바디 강제 ON
    if (ACharacter* Char = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
    {
        if (ANonCharacterBase* Non = Cast<ANonCharacterBase>(Char))
        {
            Non->SetForceFullBody(true);        // 풀바디 모션
            Non->StartAttackAlignToCamera();    // 카메라 방향으로 회전 정렬 시작
        }
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
    // 데미지 계수 계산 (DataAsset 기반)
    CurrentSkillLevel = FMath::Max(1, Level);
    CurrentDamageScale = 1.f;

    if (Row->LevelScalars.IsValidIndex(CurrentSkillLevel - 1))
    {
        CurrentDamageScale = Row->LevelScalars[CurrentSkillLevel - 1];
    }
    // 디버그 로그
    if (AActor* Avatar = ActorInfo->AvatarActor.Get())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SkillGA] Avatar=%s SkillId=%s Lv=%d DamageScale=%.2f"),
            *Avatar->GetName(),
            *SkillId.ToString(),
            CurrentSkillLevel,
            CurrentDamageScale);
    }

    // 여기서 캐릭터에 계수 전달
    if (AActor* Avatar = ActorInfo->AvatarActor.Get())
    {
        if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Avatar))
        {
            NonChar->SetLastSkillDamageScale(CurrentDamageScale);
        }
    }
    
    // === 1) SP 소모량 계산 ===
    const float StaminaCost = SkillMgr->GetStaminaCost(*Row, Level);

    if (StaminaCost > 0.f)
    {
        const FGameplayAttribute SPAttr = UNonAttributeSet::GetSPAttribute();
        const float CurrentSP = ASC->GetNumericAttribute(SPAttr);

        // SP 부족 → 스킬 실패
        if (CurrentSP < StaminaCost)
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
            return;
        }

        // 서버에서만 실제 수치 변경
        if (OwnerActor->HasAuthority())
        {
            const float NewSP = FMath::Max(0.f, CurrentSP - StaminaCost);
            ASC->SetNumericAttributeBase(SPAttr, NewSP);
        }
    }

    //  여기서 CommitAbility() 호출해서 쿨타임 GE 사용 가능
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // === 2) 몽타주 재생 ===
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

void UGA_SkillBase::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    // 스킬 GA가 끝날 때 풀바디 요청 해제 (카운터 방식이라 한 번만 호출)
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        if (ANonCharacterBase* Non = Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get()))
        {
            Non->SetForceFullBody(false);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
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
