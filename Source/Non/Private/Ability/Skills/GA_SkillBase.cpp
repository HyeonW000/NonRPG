#include "Ability/Skills/GA_SkillBase.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/NonCharacterBase.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "UI/InGameHUD.h"
#include "Core/NonUIManagerComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

// ── 헬퍼: UIManager → InGameHUD ──────────────────────────────────
namespace
{
    static UInGameHUD* GetHUDFor_SkillBase(const FGameplayAbilityActorInfo* Info)
    {
        if (!Info || !Info->AvatarActor.IsValid())
        {
            return nullptr;
        }
        if (UNonUIManagerComponent* M = Cast<UNonUIManagerComponent>(
                Info->AvatarActor.Get()->GetComponentByClass(
                    UNonUIManagerComponent::StaticClass())))
        {
            return M->GetInGameHUD();
        }
        return nullptr;
    }
}


UGA_SkillBase::UGA_SkillBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bRetriggerInstancedAbility = true; // [Combo Fix] 1타 시전 도중 2타 연계 스킬(동일 어빌리티 클래스) 재시전 허용!
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 🔥 스킬 실행 동안 캐릭터에게 State.Skill 태그 부여 (상체/풀바디 회전 판별 및 점프/상호작용 차단용)
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Skill")));

    // 💥 넉다운/스턴/사망 중에는 공격 및 스킬 발동을 원천 차단! (에디터에서도 자유롭게 추가/수정 가능)
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Knockdown")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.CrowdControl")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead")));
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

    // 🔥 풀바디 모션 vs 상체 블렌딩 모션 결정:
    // 버프 스킬이거나, Combat.bForceFullBody 가 false 인 스킬은 상체 블렌딩(Pose_UpperBodyBlend)을 유지!
    const bool bShouldForceFullBody = Row->Combat.bForceFullBody && !Row->Buff.bHasBuff;

    if (ACharacter* Char = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
    {
        if (ANonCharacterBase* Non = Cast<ANonCharacterBase>(Char))
        {
            if (bShouldForceFullBody)
            {
                Non->SetForceFullBody(true);
                bHasRequestedFullBody = true;
                Non->StartAttackAlignToCamera();
            }
            else
            {
                // 버프 및 상체 전용 스킬: 풀바디를 요청하지 않고 상체 블렌드(Pose_UpperBodyBlend) 유지!
                // ※ 상체/버프 스킬은 과거 각도 고정 정렬(StartAttackAlignToCamera)을 호출하지 않고 마우스(카메라)를 실시간 자유 추종합니다.
                bHasRequestedFullBody = false;
            }
        }
    }

    // 🔥 [New] 분노 상태(State.Rage) 전용 스킬 체크!
    if (Row->Combo.bRequiresRageState)
    {
        static const FGameplayTag RageStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
        UAbilitySystemComponent* CurrentASC = GetAbilitySystemComponentFromActorInfo();
        if (!CurrentASC || !CurrentASC->HasMatchingGameplayTag(RageStateTag))
        {
            UE_LOG(LogTemp, Warning, TEXT("[GA_SkillBase] Skill '%s' CANNOT be activated because character is NOT in State.Rage (Rage State Required)!"), *SkillId.ToString());
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
            return;
        }
    }

    const int32 Level = SkillMgr->GetSkillLevel(SkillId);
    // 데미지 계수 계산 (DataAsset 기반)
    CurrentSkillLevel = FMath::Max(1, Level);
    CurrentDamageScale = 1.f;
    float CurrentStunDuration = 0.f;

    if (Row->Combat.LevelScalars.IsValidIndex(CurrentSkillLevel - 1))
    {
        CurrentDamageScale = Row->Combat.LevelScalars[CurrentSkillLevel - 1];
    }

    // [New] 레벨별 스턴 시간 데이터가 있다면 가져오기
    if (Row->StatusEffect.StunDurations.IsValidIndex(CurrentSkillLevel - 1))
    {
        CurrentStunDuration = Row->StatusEffect.StunDurations[CurrentSkillLevel - 1];
    }

    // [New] 레벨별 상태이상 지속시간 데이터가 있다면 가져오기
    float CurrentStatusEffectDuration = 0.f;
    if (Row->StatusEffect.StatusEffectDurations.IsValidIndex(CurrentSkillLevel - 1))
    {
        CurrentStatusEffectDuration = Row->StatusEffect.StatusEffectDurations[CurrentSkillLevel - 1];
    }

    // [New] 레벨별 상태이상 발동 확률 데이터가 있다면 가져오기
    float CurrentStatusEffectChance = 0.f;
    if (Row->StatusEffect.StatusEffectChances.IsValidIndex(CurrentSkillLevel - 1))
    {
        CurrentStatusEffectChance = Row->StatusEffect.StatusEffectChances[CurrentSkillLevel - 1];
    }

    // [New] 레벨별 상태이상 수치/계수 데이터가 있다면 가져오기
    float CurrentStatusEffectValue = 0.f;
    if (Row->StatusEffect.StatusEffectValues.IsValidIndex(CurrentSkillLevel - 1))
    {
        CurrentStatusEffectValue = Row->StatusEffect.StatusEffectValues[CurrentSkillLevel - 1];
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
            NonChar->SetLastSkillSpawnClass(Row->Casting.ProjectileClass); // [New] 스폰 클래스 캐시
        }
    }
    
    // === 1) 자원 소모량 계산 및 체크 ===
    const float CostVal = SkillMgr->GetSkillCost(*Row, Level);

    if (CostVal > 0.f)
    {
        FGameplayAttribute CostAttr;
        if (Row->Cost.CostType == ESkillCostType::SP)
            CostAttr = UNonAttributeSet::GetSPAttribute();
        else if (Row->Cost.CostType == ESkillCostType::MP)
            CostAttr = UNonAttributeSet::GetMPAttribute();
        else if (Row->Cost.CostType == ESkillCostType::HP)
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



    // ── 🛡️ [New] 액티브 버프 시스템 (BuffEffect 및 BuffRadius 아군 광역 버프 적용) ──
    if (CachedRow && CachedRow->Buff.bHasBuff && CachedRow->Buff.BuffEffect && ASC)
    {
        FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
        Ctx.AddSourceObject(this);
        Ctx.AddInstigator(ActorInfo->OwnerActor.Get(), ActorInfo->AvatarActor.Get());

        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(CachedRow->Buff.BuffEffect, CurrentSkillLevel, Ctx);
        if (SpecHandle.IsValid())
        {
            FGameplayTagContainer GrantedTags;
            SpecHandle.Data->GetAllGrantedTags(GrantedTags);

            // ⏱️ [1] 데이터 에셋(DA)의 Buff.BuffDurations에 레벨별 시간이 설정되어 있다면 최우선 적용! (예: 10초, 12초, 14초)
            float BaseDuration = 0.f;
            if (CachedRow->Buff.BuffDurations.IsValidIndex(CurrentSkillLevel - 1))
            {
                BaseDuration = CachedRow->Buff.BuffDurations[CurrentSkillLevel - 1];
            }
            else
            {
                BaseDuration = SpecHandle.Data->GetDuration();
                if (BaseDuration <= 0.f) BaseDuration = 15.0f; // 기본 fallback
            }

            // ⏱️ [2] 범용 데이터 주도 지속시간 시스템 (단, State.Rage는 기본 분노 전용이므로 액티브 버프에서는 제외)
            float ExtraDuration = 0.f;
            if (SkillMgr)
            {
                static const FGameplayTag RageStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
                for (const FGameplayTag& Tag : GrantedTags)
                {
                    if (Tag == RageStateTag) continue;
                    ExtraDuration += SkillMgr->GetDurationBonusForTag(Tag);
                }
            }

            const float FinalBuffDuration = BaseDuration + ExtraDuration;
            if (FinalBuffDuration > 0.f)
            {
                // 🔥 bLockDuration = true 로 잠금을 걸어야 엔진이 블루프린트 기본값으로 덮어쓰지 않고 10/12/14초가 완벽하게 적용됨!
                SpecHandle.Data->SetDuration(FinalBuffDuration, /*bLockDuration=*/true);
            }

            // 1) 시전자 본인에게 버프 적용
            ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

            // 🌟 [HUD 연동] 시전자 화면의 버프 바에 버프 아이콘 및 지속시간 실시간 표시!
            if (UInGameHUD* HUD = GetHUDFor_SkillBase(ActorInfo))
            {
                UTexture2D* BuffIcon = CachedRow->Icon.LoadSynchronous();
                HUD->AddOrUpdateBuff(SkillId, CachedRow->DisplayName, BuffIcon, FinalBuffDuration);
            }

            // 🔥 액티브 스킬(예: GE_Berserk)로 켜지는 분노 상태는 원래 분노 시간(기본 15초 + 패시브 5/10초)으로 꽉 채워 새로고침!
            static const FGameplayTag RageStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Rage"), false);
            if (GrantedTags.HasTag(RageStateTag))
            {
                if (SkillMgr)
                {
                    SkillMgr->ActivateRageState();
                }
            }

            // 2) 광역 반경(BuffRadius > 0) 설정 시 주변 아군/파티원(ANonCharacterBase)에게도 버프 적용
            if (CachedRow->Buff.BuffRadius > 0.f && GetWorld() && ActorInfo->AvatarActor.IsValid())
            {
                const FVector Origin = ActorInfo->AvatarActor->GetActorLocation();
                TArray<FOverlapResult> Overlaps;
                FCollisionQueryParams QParams;
                QParams.AddIgnoredActor(ActorInfo->AvatarActor.Get());

                GetWorld()->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(CachedRow->Buff.BuffRadius), QParams);
                for (const FOverlapResult& Overlap : Overlaps)
                {
                    if (ANonCharacterBase* AllyChar = Cast<ANonCharacterBase>(Overlap.GetActor()))
                    {
                        if (UAbilitySystemComponent* AllyASC = AllyChar->GetAbilitySystemComponent())
                        {
                            AllyASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

                            // 아군 화면 HUD에도 버프 표시
                            if (UNonUIManagerComponent* AllyUIMgr = AllyChar->FindComponentByClass<UNonUIManagerComponent>())
                            {
                                if (UInGameHUD* AllyHUD = AllyUIMgr->GetInGameHUD())
                                {
                                    AllyHUD->AddOrUpdateBuff(SkillId, CachedRow->DisplayName, CachedRow->Icon.LoadSynchronous(), FinalBuffDuration);
                                }
                            }
                        }
                    }
                }
            }
        }
    }



    if (CachedRow->Casting.CastTime > 0.f && CachedRow->Casting.CastingMontage)
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
    // 내가 풀바디를 요청했던 경우에만 안전하게 해제
    if (bHasRequestedFullBody)
    {
        if (ActorInfo && ActorInfo->AvatarActor.IsValid())
        {
            if (ANonCharacterBase* Non = Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get()))
            {
                Non->SetForceFullBody(false);
            }
        }
        bHasRequestedFullBody = false;
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

    if (CachedRow->Combat.Montage)
    {
        UAbilityTask_PlayMontageAndWait* Task =
            UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
                this,
                NAME_None,
                CachedRow->Combat.Montage,
                1.f
            );

        if (Task)
        {
            Task->OnCompleted.AddDynamic(this, &UGA_SkillBase::OnMontageFinished);
            Task->OnBlendOut.AddDynamic(this, &UGA_SkillBase::OnMontageFinished);
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

    const float CastTime = CachedRow ? CachedRow->Casting.CastTime : 2.f;
    UAnimMontage* CastMontage = CachedRow ? CachedRow->Casting.CastingMontage.Get() : nullptr;

    if (CastMontage)
    {
        CastingTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, FName("CastingMontageTask"), CastMontage, 1.f);
        if (CastingTask)
            CastingTask->ReadyForActivation();
    }

    GetWorld()->GetTimerManager().SetTimer(
        CastingTickHandle, this, &UGA_SkillBase::CastingTick, 0.05f, true);

    if (UInGameHUD* HUD = GetHUDFor_SkillBase(CurrentActorInfo))
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

    if (UInGameHUD* HUD = GetHUDFor_SkillBase(CurrentActorInfo))
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

    if (UInGameHUD* HUD = GetHUDFor_SkillBase(CurrentActorInfo))
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
            if (CachedRow && CachedRow->Casting.CastingMontage)
            {
                Char->StopAnimMontage(CachedRow->Casting.CastingMontage);
            }
        }
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
