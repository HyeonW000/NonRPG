#include "Ability/GA_Guard.h"

#include "Character/NonCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"

UGA_Guard::UGA_Guard()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 태그는 BP에서 세팅해도 되고, 여기서 기본값 줄 수도 있음
    // 예시:
    // AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Guard")));
    // ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Active.Guard")));
}

void UGA_Guard::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ACharacter* Char = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Char)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Char);
    if (!NonChar)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 이미 가드 중이면 그냥 종료
    if (NonChar->IsGuarding())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
        return;
    }

    // 비용/쿨타임 쓰고 싶으면 여기서 CommitAbility 호출
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // === 1) 캐릭터 상태 설정 ===
    NonChar->StartGuard();



    // 유지형 GA 이므로 여기서 EndAbility 호출하지 않음
    // → 입력 Release 나 CancelAbilitiesWithTag 로 종료
}

void UGA_Guard::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    // 캐릭터 가드 해제
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(ActorInfo->AvatarActor.Get()))
        {
            // 가드 중일 때만 해제
            if (NonChar->IsGuarding())
            {
                NonChar->StopGuard();


            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
