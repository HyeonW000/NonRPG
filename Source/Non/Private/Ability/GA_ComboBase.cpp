#include "Ability/GA_ComboBase.h"

#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Character/NonCharacterBase.h"

UGA_ComboBase::UGA_ComboBase()
{
    // 콤보 GA는 인스턴스 PerActor 사용
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 태그는 전부 블루프린트에서 설정 (여기서 SetAssetTags 안 함)
}

int32 UGA_ComboBase::GetSelfComboIndex() const
{
    const FGameplayTag Tag_Combo1 = FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo1"));
    const FGameplayTag Tag_Combo2 = FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo2"));
    const FGameplayTag Tag_Combo3 = FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo3"));

    const FGameplayTagContainer& Self = GetAssetTags(); // 이 GA(블프/CPP)의 AssetTags
    if (Self.HasTagExact(Tag_Combo2)) return 2;
    if (Self.HasTagExact(Tag_Combo3)) return 3;
    return 1; // 기본 1
}

void UGA_ComboBase::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // UE_LOG(LogTemp, Warning, TEXT("[ComboGA] ActivateAbility: %s"), *GetName());

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
    ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (!Character || !ASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 1) 공격 시작 시 "부드러운 정렬" 트리거 + 풀바디 강제
    if (ANonCharacterBase* Non = Cast<ANonCharacterBase>(Character))
    {
        Non->SetForceFullBody(true);
        Non->StartAttackAlignToCamera();
    }

    // 2) 이 GA(Combo1/2/3)에 세팅된 몽타주 사용
    ActiveMontage = ComboMontage;

    if (!ActiveMontage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 3) 몽타주 재생 + 끝나면 OnMontageEnded 호출
    if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
    {
        float PlayedLen = AnimInstance->Montage_Play(ActiveMontage);

        FOnMontageEnded MontageEndDelegate;
        MontageEndDelegate.BindUObject(this, &UGA_ComboBase::OnMontageEnded);
        AnimInstance->Montage_SetEndDelegate(MontageEndDelegate, ActiveMontage);
    }
    {
        UE_LOG(LogTemp, Warning, TEXT("[ComboGA] AnimInstance is NULL"));
    }
    // 4) 런타임 상태 초기화
    bComboWindowOpen = false;
    bBufferedComboInput = false;
    bChainRequestedAtWindowEnd = false;

    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Character))
    {
        NonChar->RegisterCurrentComboAbility(this);
    }
}


void UGA_ComboBase::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    // 실제로 우리가 재생한 몽타주만 처리
    if (Montage != ActiveMontage)
        return;

    // 노티파이 끝에서 이미 체인 요청이 들어간 경우: 여기서는 마무리만
    if (bChainRequestedAtWindowEnd)
    {
        return;
    }

    // 입력 없으면 끝까지 재생 후 종료 (EndAbility는 콤보 체인 쪽에서 호출)
    if (!bBufferedComboInput)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, bInterrupted);
        return;
    }

    // (안전망) 윈도우 종료 훅을 못 받았는데 버퍼가 있는 경우만 여기서 체인
    TryActivateNextCombo();
}

void UGA_ComboBase::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 체인으로 끝나든 말든, 내가 켰던 건 내가 끈다.
    // 다음 콤보가 있다면 ActivateAbility에서 다시 켰을 것 (카운트 관리됨)
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        if (ANonCharacterBase* Non = Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get()))
        {
            Non->SetForceFullBody(false);
        }
    }

    // 한 번 썼으면 리셋
    bEndFromChain = false;

    // 등록 해제 & 재생 종료 정리
    if (Character)
    {
        if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
        {
            if (ActiveMontage && Anim->Montage_IsPlaying(ActiveMontage))
            {
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

    // 노티파이 '끝' 지점: 입력 버퍼가 있으면 바로 다음 콤보로 체인
    if (!bComboWindowOpen && bBufferedComboInput && !bChainRequestedAtWindowEnd)
    {
        bChainRequestedAtWindowEnd = true; // 중복 방지

        // 노티파이 끝에서 바로 다음 콤보 발동 (이 GA는 내부에서 종료)
        TryActivateNextCombo();
        return;
    }
}

void UGA_ComboBase::BufferComboInput()
{
    if (!bBufferedComboInput)
    {
        bBufferedComboInput = true;
    }
}

void UGA_ComboBase::TryActivateNextCombo()
{
    if (!ASC) return;

    const FGameplayTag Tag_Combo1 = FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo1"));
    const FGameplayTag Tag_Combo2 = FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo2"));
    const FGameplayTag Tag_Combo3 = FGameplayTag::RequestGameplayTag(TEXT("Ability.Combo3"));

    const FGameplayTagContainer Self = GetAssetTags();

    bool bResult = false;
    if (Self.HasTagExact(Tag_Combo1))
    {
        bResult = ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(Tag_Combo2));
    }
    else if (Self.HasTagExact(Tag_Combo2))
    {
        bResult = ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(Tag_Combo3));
    }
    else if (Self.HasTagExact(Tag_Combo3))
    {
        // 3타 이후는 추가 콤보 없음
    }

    // 다음 콤보로 "체인 성공"한 경우에는
//    이번 EndAbility 에서는 풀바디 플래그를 건드리지 않게 표시
    bEndFromChain = bResult;

    // 현재 GA는 여기서 종료 (다음 GA가 재생을 이어감)
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}
