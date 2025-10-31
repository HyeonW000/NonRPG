#include "Ability/GA_ComboBase.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/NonAnimInstance.h"
#include "Character/NonCharacterBase.h"

UGA_ComboBase::UGA_ComboBase()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // Armed 상태여야만 발동
    ActivationRequiredTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Armed")));

    // 이 GA는 '공격' 계열이다 → 태그 기반 취소/필터에 사용
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack")));

    // '가드 중'이면 발동 자체가 막히게
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Guard")));
}

void UGA_ComboBase::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // 어떤 태그로 이 GA가 정의되어 있는지 (자기 자신)
    const FGameplayTagContainer MyTags = GetAssetTags();
    UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] ActivateAbility: %s | AssetTags=%s"),
        *GetName(), *MyTags.ToStringSimple());

    // 소유자 쪽 ASC가 보유한 태그 덤프
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        FGameplayTagContainer Owned;
        ActorInfo->AbilitySystemComponent->GetOwnedGameplayTags(Owned);
        UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] Owner ASC OwnedTags=%s"),
            *Owned.ToStringSimple());
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] CommitAbility FAILED"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    ASC = ActorInfo->AbilitySystemComponent.Get();

    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // AnimBP(UNonAnimInstance)에서 콤보 1타 몽타주 가져오기
    UAnimInstance* RawAnim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
    UNonAnimInstance* NonAnim = Cast<UNonAnimInstance>(RawAnim);
    if (!NonAnim)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    ActiveMontage = NonAnim->GetComboMontage(1);
    if (!ActiveMontage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 재생은 하나의 경로로만: AnimInstance에서 ActiveMontage 재생 + EndDelegate 연결
    if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
    {
        AnimInstance->Montage_Play(ActiveMontage);

        FOnMontageEnded MontageEndDelegate;
        MontageEndDelegate.BindUObject(this, &UGA_ComboBase::OnMontageEnded);
        AnimInstance->Montage_SetEndDelegate(MontageEndDelegate, ActiveMontage);
    }

    // 런타임 상태 초기화
    bComboWindowOpen = false;
    bBufferedComboInput = false;
    bChainRequestedAtWindowEnd = false;

    // 현재 콤보 인스턴스를 캐릭터에 등록(디버그/취소용)
    if (ANonCharacterBase* Non = Cast<ANonCharacterBase>(Character))
    {
        Non->RegisterCurrentComboAbility(this);
    }

    UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] ActivateAbility: %s"), *GetName());
}

void UGA_ComboBase::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] OnMontageEnded (Interrupted=%s) Montage=%s"),
        bInterrupted ? TEXT("true") : TEXT("false"),
        *GetNameSafe(Montage));

    // 실제로 우리가 재생한 몽타주만 처리
    if (Montage != ActiveMontage)
        return;

    // 노티파이 끝에서 이미 체인 요청이 들어간 경우: 여기서는 마무리만
    if (bChainRequestedAtWindowEnd)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] Already chained at window end → just EndAbility"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, bInterrupted);
        return;
    }

    // 입력 없으면 끝까지 재생 후 종료
    if (!bBufferedComboInput)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] No buffer → EndAbility"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, bInterrupted);
        return;
    }

    // (안전망) 혹시 윈도우 종료 훅을 못 받았는데 버퍼가 있는 경우에만 여기서 체인
    UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] (Fallback) Buffered at montage end → TryActivateNextCombo"));
    TryActivateNextCombo();
}

void UGA_ComboBase::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 등록 해제 & 재생 종료 정리
    if (Character)
    {
        if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
        {
            if (ActiveMontage && Anim->Montage_IsPlaying(ActiveMontage))
            {
                // 취소로 끝났다면 즉시 끊어 UpperBody가 Guard 등으로 바로 넘어가게 함
                const float BlendOut = bWasCancelled ? 0.02f : 0.15f;
                Anim->Montage_Stop(BlendOut, ActiveMontage);
            }
        }

        if (ANonCharacterBase* Non = Cast<ANonCharacterBase>(Character))
        {
            Non->UnregisterCurrentComboAbility(this);
        }
    }

    bComboWindowOpen = false;
    bBufferedComboInput = false;
    bChainRequestedAtWindowEnd = false;
    ActiveMontage = nullptr;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ComboBase::SetComboWindowOpen(bool bOpen)
{
    bComboWindowOpen = bOpen;
    UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] ComboWindowOpen: %s"), bOpen ? TEXT("true") : TEXT("false"));

    // 노티파이 '끝' 지점: 입력 버퍼가 있으면 바로 다음 콤보로 체인
    if (!bComboWindowOpen && bBufferedComboInput && !bChainRequestedAtWindowEnd)
    {
        bChainRequestedAtWindowEnd = true; // 중복 방지
        UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] WindowEnd + Buffered → Chain next combo at window end"));

        // 노티파이 끝에서 바로 다음 콤보 발동 (이 GA는 내부에서 종료)
        TryActivateNextCombo();
        return;
    }

    // 입력이 없으면 아무 것도 안 함 → 현재 몽타주가 자연스럽게 끝나고 OnMontageEnded에서 EndAbility
}

void UGA_ComboBase::BufferComboInput()
{
    // 즉시 체인 금지. 버퍼만 세팅.
    if (!bBufferedComboInput)
    {
        bBufferedComboInput = true;
        UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] BufferComboInput → buffered"));
    }
}

void UGA_ComboBase::TryActivateNextCombo()
{
    if (!ASC) return;

    // 중앙 집계 태그 사용
    const FGameplayTag Tag_Combo1 = FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo1"));
    const FGameplayTag Tag_Combo2 = FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo2"));
    const FGameplayTag Tag_Combo3 = FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo3"));

    const bool bHas1 = GetAssetTags().HasTagExact(Tag_Combo1);
    const bool bHas2 = GetAssetTags().HasTagExact(Tag_Combo2);
    const bool bHas3 = GetAssetTags().HasTagExact(Tag_Combo3);

    UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] TryActivateNextCombo: SelfTags -> C1=%d C2=%d C3=%d"), bHas1, bHas2, bHas3);

    FGameplayTagContainer Owned;
    ASC->GetOwnedGameplayTags(Owned);
    UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] ASC OwnedTags=%s"), *Owned.ToStringSimple());

    bool bResult = false;
    if (bHas1)
    {
        bResult = ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(Tag_Combo2));
        UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] TryActivate Combo2 -> %s"), bResult ? TEXT("OK") : TEXT("FAIL"));
    }
    else if (bHas2)
    {
        bResult = ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(Tag_Combo3));
        UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] TryActivate Combo3 -> %s"), bResult ? TEXT("OK") : TEXT("FAIL"));
    }
    else if (bHas3)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_ComboBase] Already Combo3 (last)"));
    }

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}
