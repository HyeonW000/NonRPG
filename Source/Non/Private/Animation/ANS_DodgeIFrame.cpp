#include "Animation/ANS_DodgeIFrame.h"
#include "Core/NonGameplayTags.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Character/NonCharacterBase.h"

// 전역 static 객체 생성은 태그 매니저 초기화 이전 시점에 호출될 수 있어 크래시를 유발하므로 제거합니다.
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
                FGameplayTag IFrameTag = NonTags::State_IFrame.GetTag();
                ASC->AddLooseGameplayTag(IFrameTag);
            }
        }

        // 저스트 회피 판정 기준 시각 기록 (설계서 5장 16)
        if (bRecordJustDodge)
        {
            if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Owner))
            {
                NonChar->NotifyDodgeIFrameStart();
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
                Cap->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
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
                FGameplayTag IFrameTag = NonTags::State_IFrame.GetTag();
                ASC->RemoveLooseGameplayTag(IFrameTag);
            }
        }
    }
}