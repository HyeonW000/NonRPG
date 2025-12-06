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
    // 기본 값은 그냥 "상호작용" 정도로만.
    // 실제 문구는 BP에서 GetInteractLabel 이벤트로 오버라이드해서 사용.
    return FText::FromString(TEXT("상호작용"));
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
        UE_LOG(LogTemp, Warning, TEXT("[Pickup] %s: Interactor has no InventoryComponent"), *GetName());
        return;
    }

    if (ItemId.IsNone() || Count <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pickup] %s: Invalid ItemId/Count"), *GetName());
        return;
    }

    int32 LastSlot = INDEX_NONE;
    const bool bAdded = Inv->AddItem(ItemId, Count, LastSlot);
    if (bAdded)
    {
        UE_LOG(LogTemp, Log, TEXT("[Pickup] %s picked up %d x %s (Slot=%d)"),
            *Interactor->GetName(), Count, *ItemId.ToString(), LastSlot);

        Destroy();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pickup] Inventory full, cannot pick up %s"), *ItemId.ToString());
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
