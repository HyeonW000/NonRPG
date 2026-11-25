#include "Ability/NonAbilitySystemComponent.h"
#include "Core/NonGameplayTags.h"

UNonAbilitySystemComponent::UNonAbilitySystemComponent()
{
    // 커스텀 설정 필요 시 여기에
}

bool UNonAbilitySystemComponent::AreAbilityTagsBlocked(const FGameplayTagContainer& Tags) const
{
    if (Super::AreAbilityTagsBlocked(Tags))
    {
        return true;
    }

    if (HasMatchingGameplayTag(NonTags::State_Grabbed.GetTag()))
    {
        FGameplayTagContainer AllowedWhileGrabbed;
        AllowedWhileGrabbed.AddTag(NonTags::State_Grabbed.GetTag());   // GA_Player_Grabbed (잡힘 몽타주)
        AllowedWhileGrabbed.AddTag(NonTags::Ability_Death.GetTag());   // 사망
        AllowedWhileGrabbed.AddTag(NonTags::Ability_Revive.GetTag());  // 부활
        return !Tags.HasAny(AllowedWhileGrabbed);
    }
    return false;
}
