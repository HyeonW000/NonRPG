#include "Animation/AnimNotify_SwapWeaponSocket.h"
#include "Character/NonCharacterBase.h"
#include "Equipment/EquipmentComponent.h"
#include "Inventory/InventoryItem.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"

FString UAnimNotify_SwapWeaponSocket::GetNotifyName_Implementation() const
{
    switch (Mode)
    {
    case EEquipAttachMode::ToHand:  return TEXT("Attach→Hand");
    case EEquipAttachMode::ToHome:  return TEXT("Attach→Home");
    case EEquipAttachMode::ToNamed: return FString::Printf(TEXT("Attach→%s"), *NamedSocket.ToString());
    }
    return Super::GetNotifyName_Implementation();
}

void UAnimNotify_SwapWeaponSocket::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (!MeshComp) return;

    ANonCharacterBase* Char = MeshComp->GetOwner() ? Cast<ANonCharacterBase>(MeshComp->GetOwner()) : nullptr;
    if (!Char) return;

    UEquipmentComponent* Eq = Char->FindComponentByClass<UEquipmentComponent>();
    if (!Eq) return;

    UInventoryItem* Item = Eq->GetEquippedItemBySlot(Slot);
    if (!Item) return;

    FName TargetSocket = NAME_None;

    switch (Mode)
    {
    case EEquipAttachMode::ToHand:
        TargetSocket = Char->GetHandSocketForItem(Item);
        break;

    case EEquipAttachMode::ToHome:
    {
        TargetSocket = Eq->GetHomeSocketForSlot(Slot);
        if (TargetSocket == NAME_None)
        {
            TargetSocket = Char->GetHomeFallbackSocketForItem(Item);
        }
        break;
    }

    case EEquipAttachMode::ToNamed:
        TargetSocket = NamedSocket;
        break;
    }

    if (TargetSocket != NAME_None)
    {
        Eq->ReattachSlotToSocket(Slot, TargetSocket, FTransform::Identity);
    }

    // 무장 상태/스탠스 갱신
    if (Mode == EEquipAttachMode::ToHand)
    {
        Char->SetArmed(true);
    }
    else if (Mode == EEquipAttachMode::ToHome)
    {
        Char->SetArmed(false);
    }
    else
    {
        Char->RefreshWeaponStance();
    }

    if (!bApplyWeaponStateTag)
    {
        return;
    }

    if (!Char->HasAuthority())
    {
        return;
    }

    UAbilitySystemComponent* ASC = nullptr;

    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Char))
    {
        ASC = ASI->GetAbilitySystemComponent();
    }
    else
    {
        ASC = Char->FindComponentByClass<UAbilitySystemComponent>();
    }

    if (!ASC)
    {
        return;
    }

    // 먼저 지울 태그들
    if (TagsToClear.Num() > 0)
    {
        FGameplayTagContainer ToRemove;
        for (const FGameplayTag& T : TagsToClear)
        {
            if (T.IsValid())
            {
                ToRemove.AddTag(T);
            }
        }

        if (!ToRemove.IsEmpty())
        {
            ASC->RemoveLooseGameplayTags(ToRemove);
        }
    }

    // 새로 추가할 태그
    for (const FGameplayTag& T : TagsToAdd)
    {
        if (T.IsValid())
        {
            ASC->AddLooseGameplayTag(T);
        }
    }
}
