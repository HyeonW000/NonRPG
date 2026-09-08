#include "Ability/Skills/GA_Berserk.h"
#include "Skill/SkillManagerComponent.h"
#include "Character/NonCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

UGA_Berserk::UGA_Berserk()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGA_Berserk::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    ANonCharacterBase* CasterChar = Cast<ANonCharacterBase>(GetAvatarActorFromActorInfo());
    if (!CasterChar)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    const FVector CasterLoc = CasterChar->GetActorLocation();

    // 1. 🔥 [분노 부여] 시전 시 즉시 '분노'(State.Rage / 기본 15초 + 패시브 5/10초) 버프 획득!
    if (USkillManagerComponent* SkillMgr = CasterChar->FindComponentByClass<USkillManagerComponent>())
    {
        SkillMgr->ActivateRageState();
    }

    // 2. 💥 [주변 적 비틀거림/경직 연출] 반경 4m 내 적들에게 피격 이벤트(GA_HitReaction) 발송!
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(CasterChar);

    World->OverlapMultiByChannel(Overlaps, CasterLoc, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(StaggerRadius), QueryParams);

    static const FGameplayTag HitLightTag = FGameplayTag::RequestGameplayTag(TEXT("Effect.Hit.Light.Front"), false);

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* OverlappedActor = Overlap.GetActor();
        if (OverlappedActor && OverlappedActor != CasterChar)
        {
            if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OverlappedActor))
            {
                // 적 캐릭터에게 약피격 몽타주 비틀거림 이벤트 발송!
                FGameplayEventData EventData;
                EventData.EventTag = HitLightTag;
                EventData.Instigator = CasterChar;
                EventData.Target = OverlappedActor;

                TargetASC->HandleGameplayEvent(HitLightTag, &EventData);
                UE_LOG(LogTemp, Warning, TEXT("[GA_Berserk Debug] Staggered Enemy %s in %.1fm radius!"), *OverlappedActor->GetName(), StaggerRadius / 100.f);
            }
        }
    }

    // 3. 🛡️ [10m 아군 버프] 반경 10m 내 아군들에게 10초간 물리/마법 공격력 +10% 버프 부여!
    TSubclassOf<UGameplayEffect> EffectiveBuffClass = PartyBuffEffectClass;
    if (!EffectiveBuffClass && CachedRow && CachedRow->Buff.BuffEffect)
    {
        EffectiveBuffClass = CachedRow->Buff.BuffEffect;
    }

    if (EffectiveBuffClass)
    {
        const float ActualRadius = (CachedRow && CachedRow->Buff.BuffRadius > 0.f) ? CachedRow->Buff.BuffRadius : PartyBuffRadius;
        TArray<FOverlapResult> PartyOverlaps;
        World->OverlapMultiByChannel(PartyOverlaps, CasterLoc, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(ActualRadius), QueryParams);

        for (const FOverlapResult& Overlap : PartyOverlaps)
        {
            if (ANonCharacterBase* AllyChar = Cast<ANonCharacterBase>(Overlap.GetActor()))
            {
                if (AllyChar->GetAbilitySystemComponent())
                {
                    FGameplayEffectContextHandle Ctx = AllyChar->GetAbilitySystemComponent()->MakeEffectContext();
                    Ctx.AddInstigator(CasterChar, CasterChar->GetController());

                    FGameplayEffectSpecHandle SpecHandle = AllyChar->GetAbilitySystemComponent()->MakeOutgoingSpec(EffectiveBuffClass, CurrentSkillLevel, Ctx);
                    if (SpecHandle.IsValid())
                    {
                        AllyChar->GetAbilitySystemComponent()->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
                        UE_LOG(LogTemp, Warning, TEXT("[GA_Berserk Debug] Applied Party Buff to Ally %s!"), *AllyChar->GetName());
                    }
                }
            }
        }
    }

    // 몽타주가 지정되어 있지 않다면 즉시 어빌리티 종료. 지정되어 있다면 몽타주 재생이 끝날 때 Super::OnMontageFinished()에서 종료
    if (!CachedRow || !CachedRow->Combat.Montage)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}
