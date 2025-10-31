#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Inventory/ItemEnums.h"
#include "AnimNotify_SwapWeaponSocket.generated.h"

UENUM(BlueprintType)
enum class EEquipAttachMode : uint8
{
    ToHand  UMETA(DisplayName = "To Hand Socket"),
    ToHome  UMETA(DisplayName = "To Home Socket"),
    ToNamed UMETA(DisplayName = "To Named Socket")
};

UCLASS(DisplayName = "Swap Weapon Socket")
class NON_API UAnimNotify_SwapWeaponSocket : public UAnimNotify
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach")
    EEquipAttachMode Mode = EEquipAttachMode::ToHand;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach", meta = (EditCondition = "Mode==EEquipAttachMode::ToNamed"))
    FName NamedSocket;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach")
    EEquipmentSlot Slot = EEquipmentSlot::WeaponMain;

    virtual FString GetNotifyName_Implementation() const override;
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
