#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Inventory/ItemEnums.h"
#include "GameplayTagContainer.h"
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

    // 태그 관련 옵션
    /** 이 Notify에서 무기 상태 태그를 적용할지 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponState")
    bool bApplyWeaponStateTag = false;

    /** 이번 Notify에서 추가할 태그 (예: State.Armed.OneHand / State.Armed.TwoHand 등) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponState", meta = (EditCondition = "bApplyWeaponStateTag"))
    TArray<FGameplayTag> TagsToAdd;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponState", meta = (EditCondition = "bApplyWeaponStateTag"))
    TArray<FGameplayTag> TagsToClear;

    virtual FString GetNotifyName_Implementation() const override;
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
