#include "Ability/GA_ToggleWeapon.h"

#include "Character/NonCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "Inventory/InventoryItem.h"
#include "Components/MeshComponent.h"

UGA_ToggleWeapon::UGA_ToggleWeapon()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 태그는 BP에서 세팅해도 되고, 여기서 기본값을 줄 수도 있음
    // 예)
    // AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.ToggleWeapon")));
}

void UGA_ToggleWeapon::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(Character);
    if (!NonChar)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }



    CachedNonChar = NonChar;

    // ── 1) 이번 토글에서의 "목표 무장 상태" 계산 ──
    bTargetArmed = !NonChar->IsArmed();

    // [New] 방패 등 다른 요인으로 Armed 상태라도, 메인 무기가 아직 쉬스(집)에 있다면 Draw 해야 함
    if (UEquipmentComponent* EquipComp = NonChar->FindComponentByClass<UEquipmentComponent>())
    {
        if (UInventoryItem* MainItem = EquipComp->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain))
        {
            // 메인 무기가 있는데, 아직 손에 없다면 -> Armed 목표 (꺼내기)
            if (UMeshComponent* MainMesh = EquipComp->GetVisualComponent(EEquipmentSlot::WeaponMain))
            {
                const FName HandSocket = NonChar->GetHandSocketForItem(MainItem);
                if (MainMesh->GetAttachSocketName() != HandSocket)
                {
                    bTargetArmed = true; 
                    UE_LOG(LogTemp, Warning, TEXT("[GA_ToggleWeapon] Main Weapon is not in hand -> Force Draw"));
                }
            }
        }
    }
    
    UAnimMontage* SelectedMontage = nullptr;

    if (bTargetArmed)
    {
        // 무장 쪽 몽타주 하나만 사용 (원하는 쪽 골라)
        SelectedMontage = DrawMontage;   // 또는 DrawUpperBodyMontage
    }
    else
    {
        // 비무장 쪽 몽타주 하나만 사용
        SelectedMontage = SheatheMontage; // 또는 SheatheUpperBodyMontage
    }

    if (SelectedMontage)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_ToggleWeapon] Class: %s, Playing Montage: %s"), *GetName(), *SelectedMontage->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[GA_ToggleWeapon] Class: %s, No Montage Selected!"), *GetName());
    }

    // ── 3) 몽타주 재생 ──
    UAnimInstance* AnimInstance = Character->GetMesh()
        ? Character->GetMesh()->GetAnimInstance()
        : nullptr;

    if (!AnimInstance)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ActiveMontage = SelectedMontage;

    AnimInstance->Montage_Play(SelectedMontage, 1.0f);

    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &UGA_ToggleWeapon::OnToggleMontageEnded);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, SelectedMontage);
}

void UGA_ToggleWeapon::OnToggleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage != ActiveMontage)
    {
        return;
    }

    // 이제 무장 상태는 AnimNotify_SwapWeaponSocket에서 이미 처리함
    ActiveMontage = nullptr;
    CachedNonChar = nullptr;

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bInterrupted, false);
}




