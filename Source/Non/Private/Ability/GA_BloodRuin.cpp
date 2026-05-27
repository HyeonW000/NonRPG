#include "Ability/GA_BloodRuin.h"
#include "AbilitySystemComponent.h"

UGA_BloodRuin::UGA_BloodRuin()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    RequiredStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Ability.Frenzy"), false);
}

bool UGA_BloodRuin::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        if (ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(RequiredStateTag))
        {
            return true;
        }
    }

    return false;
}
