#include "Animation/AnimNotify_ReleaseGrab.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Ability/GA_Boss_Grab.h"

void UAnimNotify_ReleaseGrab::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp) return;

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner) return;

    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner))
    {
        if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
        {
            TArray<FGameplayAbilitySpec>& Specs = ASC->GetActivatableAbilities();
            for (FGameplayAbilitySpec& Spec : Specs)
            {
                UGA_Boss_Grab* TargetGrabGA = Cast<UGA_Boss_Grab>(Spec.GetPrimaryInstance());
                if (!TargetGrabGA)
                {
                    TargetGrabGA = Cast<UGA_Boss_Grab>(Spec.Ability);
                }
                if (!TargetGrabGA)
                {
                    for (UGameplayAbility* ActiveInst : Spec.GetAbilityInstances())
                    {
                        if (UGA_Boss_Grab* Found = Cast<UGA_Boss_Grab>(ActiveInst))
                        {
                            TargetGrabGA = Found;
                            break;
                        }
                    }
                }

                if (TargetGrabGA)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[AnimNotify_ReleaseGrab] 내팽개치기 순간 유저 손 소켓 분리(Detach) 및 던지기 수행!"));
                    TargetGrabGA->ReleaseGrabbedVictim();
                    return;
                }
            }
        }
    }
}
