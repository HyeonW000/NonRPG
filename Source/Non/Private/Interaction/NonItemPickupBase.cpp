#include "Interaction/NonItemPickupBase.h"

#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "Character/NonCharacterBase.h"
#include "Inventory/InventoryComponent.h"

ANonItemPickupBase::ANonItemPickupBase()
{
    PrimaryActorTick.bCanEverTick = false;

    // 1) 상호작용 전용 콜리전
    InteractCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractCollision"));
    RootComponent = InteractCollision;

    InteractCollision->SetSphereRadius(50.f);
    InteractCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractCollision->SetCollisionObjectType(ECC_WorldDynamic);
    InteractCollision->SetCollisionResponseToAllChannels(ECR_Ignore);

    // 프로젝트에서 만든 Interact Trace 채널 (예: GameTraceChannel1)
    InteractCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);

    // 2) 보여줄 무기 메쉬
    ItemMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ItemMesh"));
    ItemMesh->SetupAttachment(RootComponent);
    ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    bReplicates = true;
}

FText ANonItemPickupBase::GetInteractLabel_Implementation()
{
    return FText::FromString(TEXT("획득하기"));
}

void ANonItemPickupBase::Interact_Implementation(ANonCharacterBase* Interactor)
{
    if (!Interactor)
    {
        return;
    }

    UInventoryComponent* Inv = Interactor->FindComponentByClass<UInventoryComponent>();
    if (!Inv)
    {

        return;
    }

    if (ItemId.IsNone() || Count <= 0)
    {

        return;
    }

    int32 LastSlot = INDEX_NONE;
    const bool bAdded = Inv->AddItem(ItemId, Count, LastSlot);
    if (bAdded)
    {


        Destroy();
    }
    else
    {

    }
}

void ANonItemPickupBase::SetInteractHighlight_Implementation(bool bEnable)
{
    if (ItemMesh)
    {
        ItemMesh->SetRenderCustomDepth(bEnable);
        if (bEnable)
        {
            ItemMesh->SetCustomDepthStencilValue(1);
        }
    }
}
