#include "Ability/Skills/GA_SkillBase.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/NonCharacterBase.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "UI/InGameHUD.h"
#include "Core/NonUIManagerComponent.h"

// ── 헬퍼: UIManager → InGameHUD ──────────────────────────────────
static UInGameHUD *GetHUDFor(const FGameplayAbilityActorInfo *Info) {
  if (!Info || !Info->AvatarActor.IsValid())
    return nullptr;
  if (UNonUIManagerComponent *M = Cast<UNonUIManagerComponent>(
          Info->AvatarActor.Get()->GetComponentByClass(
              UNonUIManagerComponent::StaticClass())))
    return M->GetInGameHUD();
  return nullptr;
}

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
    float CurrentStunDuration = 0.f;

    if (Row->LevelScalars.IsValidIndex(CurrentSkillLevel - 1))
    {
        CurrentDamageScale = Row->LevelScalars[CurrentSkillLevel - 1];
    }

    // [New] 레벨별 스턴 시간 데이터가 있다면 가져오기
    if (Row->StunDurations.IsValidIndex(CurrentSkillLevel - 1))
    {
        CurrentStunDuration = Row->StunDurations[CurrentSkillLevel - 1];
    }

    // [New] 레벨별 상태이상 지속시간 데이터가 있다면 가져오기
    float CurrentStatusEffectDuration = 0.f;
    if (Row->StatusEffectDurations.IsValidIndex(CurrentSkillLevel - 1))
    {
        CurrentStatusEffectDuration = Row->StatusEffectDurations[CurrentSkillLevel - 1];
    }

    // [New] 레벨별 상태이상 발동 확률 데이터가 있다면 가져오기
    float CurrentStatusEffectChance = 0.f;
    if (Row->StatusEffectChances.IsValidIndex(CurrentSkillLevel - 1))
    {
        CurrentStatusEffectChance = Row->StatusEffectChances[CurrentSkillLevel - 1];
    }

    // [New] 레벨별 상태이상 수치/계수 데이터가 있다면 가져오기
    float CurrentStatusEffectValue = 0.f;
    if (Row->StatusEffectValues.IsValidIndex(CurrentSkillLevel - 1))
    {
        CurrentStatusEffectValue = Row->StatusEffectValues[CurrentSkillLevel - 1];
    }

    // 여기서 캐릭터에 계수 및 레벨 전달
    if (AActor* Avatar = ActorInfo->AvatarActor.Get())
    {
        if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Avatar))
        {
            NonChar->SetLastSkillDamageScale(CurrentDamageScale);
            NonChar->SetLastSkillLevel(CurrentSkillLevel); // [New] 레벨 저장
            NonChar->SetLastSkillStunDuration(CurrentStunDuration); // [New] 스턴 시간 저장
            NonChar->SetLastSkillStatusEffectDuration(CurrentStatusEffectDuration); // [New] 상태이상 시간 저장
            NonChar->SetLastSkillStatusEffectChance(CurrentStatusEffectChance); // [New] 상태이상 확률 저장
            NonChar->SetLastSkillStatusEffectValue(CurrentStatusEffectValue); // [New] 상태이상 수치 저장
            NonChar->SetLastSkillSpawnClass(Row->ProjectileClass); // [New] 스폰 클래스 캐시
        }
    }
    
    // === 1) 자원 소모량 계산 및 체크 ===
    const float CostVal = SkillMgr->GetSkillCost(*Row, Level);

    if (CostVal > 0.f)
    {
        FGameplayAttribute CostAttr;
        if (Row->CostType == ESkillCostType::SP)
            CostAttr = UNonAttributeSet::GetSPAttribute();
        else if (Row->CostType == ESkillCostType::MP)
            CostAttr = UNonAttributeSet::GetMPAttribute();
        else if (Row->CostType == ESkillCostType::HP)
            CostAttr = UNonAttributeSet::GetHPAttribute();

        if (CostAttr.IsValid())
        {
            const float CurrentVal = ASC->GetNumericAttribute(CostAttr);

            // 자원 부족 → 스킬 실패
            if (CurrentVal < CostVal)
            {
                EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
                return;
            }

            // 서버에서만 실제 수치 변경
            if (OwnerActor->HasAuthority())
            {
                const float NewVal = FMath::Max(0.f, CurrentVal - CostVal);
                ASC->SetNumericAttributeBase(CostAttr, NewVal);
            }
        }
    }

    //  여기서 CommitAbility() 호출해서 쿨타임 GE 사용 가능
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    CachedRow = Row;

    // [New] 캐스팅/시전 시작 시점에 크로스헤어가 가리키는 조준점(타겟) 미리 계산 및 락온
    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get()))
    {
        APlayerController* PC = Cast<APlayerController>(NonChar->GetController());
        if (PC)
        {
            FVector CameraLoc;
            FRotator CameraRot;
            PC->GetPlayerViewPoint(CameraLoc, CameraRot);
            
            FVector Start = CameraLoc;
            FVector End = Start + (CameraRot.Vector() * 2500.f); // 사거리
            
            FHitResult HitResult;
            FCollisionQueryParams TraceParams;
            TraceParams.AddIgnoredActor(NonChar);
            
            FVector TargetLoc = End;
            AActor* TargetActor = nullptr;
            
            FCollisionShape SphereShape = FCollisionShape::MakeSphere(50.f);
            if (GetWorld()->SweepSingleByChannel(HitResult, Start, End, FQuat::Identity, ECC_Pawn, SphereShape, TraceParams))
            {
                TargetLoc = HitResult.ImpactPoint;
                TargetActor = HitResult.GetActor();
            }
            else
            {
                if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, TraceParams))
                {
                    TargetLoc = HitResult.ImpactPoint;
                }
            }
            
            NonChar->SetLastSkillTargetLocation(TargetLoc);
            NonChar->SetLastSkillTargetActor(TargetActor);
        }
    }



    if (CachedRow->CastTime > 0.f && CachedRow->CastingMontage)
    {
        StartCastingPhase();
    }
    else
    {
        PlayShootMontage();
    }
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

void UGA_SkillBase::PlayShootMontage()
{
    if (!CachedRow || !CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    if (CachedRow->Montage)
    {
        UAbilityTask_PlayMontageAndWait* Task =
            UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
                this,
                NAME_None,
                CachedRow->Montage,
                1.f
            );

        if (Task)
        {
            Task->OnCompleted.AddDynamic(this, &UGA_SkillBase::OnMontageFinished);
            Task->OnCancelled.AddDynamic(this, &UGA_SkillBase::OnMontageCancelled);
            Task->OnInterrupted.AddDynamic(this, &UGA_SkillBase::OnMontageCancelled);

            Task->ReadyForActivation();
            return;
        }
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGA_SkillBase::StartCastingPhase()
{
    RegisterHitCancelListener();

    const float CastTime = CachedRow ? CachedRow->CastTime : 2.f;
    UAnimMontage* CastMontage = CachedRow ? CachedRow->CastingMontage.Get() : nullptr;

    if (CastMontage)
    {
        CastingTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("CastingMontageTask"), CastMontage, 1.f);
        if (CastingTask)
            CastingTask->ReadyForActivation();
    }

    GetWorld()->GetTimerManager().SetTimer(
        CastingTickHandle, this, &UGA_SkillBase::CastingTick, 0.05f, true);

    if (UInGameHUD* HUD = GetHUDFor(CurrentActorInfo))
    {
        HUD->StartCasting(CastTime);
    }

    if (CastTime <= 0.f)
        OnCastingTimerExpired();
    else
        GetWorld()->GetTimerManager().SetTimer(
            CastTimerHandle, this, &UGA_SkillBase::OnCastingTimerExpired,
            CastTime, false);
}

void UGA_SkillBase::OnCastingTimerExpired()
{
    GetWorld()->GetTimerManager().ClearTimer(CastingTickHandle);
    UnregisterHitCancelListener();

    if (UInGameHUD* HUD = GetHUDFor(CurrentActorInfo))
    {
        HUD->StopCasting();
    }

    // 캐스팅 태스크가 돌고 있다면 안전하게 종료시킵니다.
    if (CastingTask)
    {
        CastingTask->EndTask();
        CastingTask = nullptr;
    }

    PlayShootMontage();
}

void UGA_SkillBase::CastingTick()
{
    if (!CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid())
        return;
    APawn* Pawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
    if (Pawn && Pawn->GetVelocity().SizeSquared2D() > 100.f * 100.f)
        OnCancelCasting();
}

void UGA_SkillBase::RegisterHitCancelListener()
{
    UAbilitySystemComponent* ASC =
        CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC)
        return;
    const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.HitReacting"));
    HitTagEventHandle =
        ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &UGA_SkillBase::OnHitTagChanged);
}

void UGA_SkillBase::UnregisterHitCancelListener()
{
    UAbilitySystemComponent* ASC =
        CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC)
        return;
    const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.HitReacting"));
    ASC->UnregisterGameplayTagEvent(HitTagEventHandle, Tag, EGameplayTagEventType::NewOrRemoved);
}

void UGA_SkillBase::OnHitTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0 && IsActive())
        OnCancelCasting();
}

void UGA_SkillBase::OnCancelCasting()
{
    GetWorld()->GetTimerManager().ClearTimer(CastTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(CastingTickHandle);
    UnregisterHitCancelListener();

    if (UInGameHUD* HUD = GetHUDFor(CurrentActorInfo))
    {
        HUD->StopCasting();
    }

    if (CastingTask)
    {
        CastingTask->EndTask();
        CastingTask = nullptr;
    }

    if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
    {
        if (ACharacter* Char = Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get()))
        {
            if (CachedRow && CachedRow->CastingMontage)
            {
                Char->StopAnimMontage(CachedRow->CastingMontage);
            }
        }
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
