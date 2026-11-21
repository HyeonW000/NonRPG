#include "Animation/AnimNotify_SwapWeaponSocket.h"
#include "Character/NonCharacterBase.h"
#include "Equipment/EquipmentComponent.h"
#include "Inventory/InventoryItem.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"

UAnimNotify_SwapWeaponSocket::UAnimNotify_SwapWeaponSocket()
{
    // 게임플레이 상태(무장/태그/소켓)를 변경하는 핵심 노티파이이므로
    // 프레임 드랍이나 블렌딩 구간에서도 절대 스킵되지 않도록 BranchingPoint 활성화
    bIsNativeBranchingPoint = true;
}

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

void UAnimNotify_SwapWeaponSocket::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
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
        // ToNamed 모드라도 메인 무기를 손 소켓으로 옮겼다면 Armed=true, 홈 소켓이면 Armed=false 처리
        if (Slot == EEquipmentSlot::WeaponMain && TargetSocket != NAME_None)
        {
            const FName HandSocket = Char->GetHandSocketForItem(Item);
            if (TargetSocket == HandSocket || TargetSocket.ToString().Contains(TEXT("hand")))
            {
                Char->SetArmed(true);
            }
            else
            {
                Char->SetArmed(false);
            }
        }
        Char->RefreshWeaponStance();
    }

    // 소켓 및 무장 상태 변경 직후 태그 강제 동기화
    Eq->UpdateWeaponTags(Char->IsArmed());
}
