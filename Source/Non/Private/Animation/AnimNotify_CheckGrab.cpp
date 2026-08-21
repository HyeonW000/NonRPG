#include "Animation/AnimNotify_CheckGrab.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Ability/GA_Boss_Grab.h"

void UAnimNotify_CheckGrab::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
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
                // Spec.IsActive() 필터링에 종속되지 않고 UGA_Boss_Grab 인스턴스 정밀 추출
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
                    TargetGrabGA->CheckAndExecuteGrab();
                    return;
                }
            }
        }
    }
}
