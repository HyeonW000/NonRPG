#include "Animation/AnimNotify_SwapWeaponSocket.h"
#include "Character/NonCharacterBase.h"
#include "Equipment/EquipmentComponent.h"
#include "Inventory/InventoryItem.h"

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
    // 이 프레임에 스탠스 동기화
    Char->RefreshWeaponStance();
}
