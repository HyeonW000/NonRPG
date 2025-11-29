#include "Animation/ANS_DodgeIFrame.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"

static FGameplayTag TAG_IFrame = FGameplayTag::RequestGameplayTag(TEXT("State.IFrame"));

void UANS_DodgeIFrame::NotifyBegin(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    float TotalDuration,
    const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
    if (!MeshComp) return;

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner) return;

    // 캡슐 비활성 옵션
    if (bDisablePawnCollision)
    {
        if (ACharacter* C = Cast<ACharacter>(Owner))
        {
            if (UCapsuleComponent* Cap = C->GetCapsuleComponent())
            {
                Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }
    }

    // 서버 권한에서만 태그 Add (서버에서 데미지 판정하므로)
    if (Owner->HasAuthority() && bAddIFrameTag)
    {
        if (IAbilitySystemInterface* Iface = Cast<IAbilitySystemInterface>(Owner))
        {
            if (UAbilitySystemComponent* ASC = Iface->GetAbilitySystemComponent())
            {
                ASC->AddLooseGameplayTag(TAG_IFrame);
            }
        }
    }
}

void UANS_DodgeIFrame::NotifyEnd(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::NotifyEnd(MeshComp, Animation, EventReference);
    if (!MeshComp) return;

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner) return;

    // 캡슐 복구
    if (bDisablePawnCollision)
    {
        if (ACharacter* C = Cast<ACharacter>(Owner))
        {
            if (UCapsuleComponent* Cap = C->GetCapsuleComponent())
            {
                Cap->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            }
        }
    }

    // 서버 권한에서만 태그 Remove
    if (Owner->HasAuthority() && bAddIFrameTag)
    {
        if (IAbilitySystemInterface* Iface = Cast<IAbilitySystemInterface>(Owner))
        {
            if (UAbilitySystemComponent* ASC = Iface->GetAbilitySystemComponent())
            {
                ASC->RemoveLooseGameplayTag(TAG_IFrame);
            }
        }
    }
}