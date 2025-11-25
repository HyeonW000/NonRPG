#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuickSlotManager.generated.h"

class UInventoryComponent;
class UInventoryItem;

USTRUCT(BlueprintType)
struct FQuickSlotEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 SlotIndex = INDEX_NONE; // 0~9
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TWeakObjectPtr<UInventoryComponent> Inventory;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGuid ItemInstanceId;
    // 이 슬롯이 가리키는 "아이템 타입" (수량 0이어도 유지)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ItemId = NAME_None;

};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuickSlotChanged, int32, SlotIndex, UInventoryItem*, Item);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class NON_API UQuickSlotManager : public UActorComponent
{
    GENERATED_BODY()
public:
    UQuickSlotManager();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickSlot")
    int32 NumSlots = 10;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuickSlot")
    TArray<FQuickSlotEntry> Slots;

    // 슬롯별 스킬 ID
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuickSlot|Skill")
    TArray<FName> SkillIdsPerSlot;

    UPROPERTY(BlueprintAssignable, Category = "QuickSlot")
    FOnQuickSlotChanged OnQuickSlotChanged;

    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    bool AssignFromInventory(int32 QuickIndex, UInventoryComponent* SourceInv, int32 SourceIdx);

    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    bool ClearSlot(int32 QuickIndex);

    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    bool UseQuickSlot(int32 QuickIndex);

    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    bool SwapSlots(int32 A, int32 B);

    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    int32 FindSlotByItemId(FName ItemId) const;

    UFUNCTION(BlueprintPure, Category = "QuickSlot")
    UInventoryItem* ResolveItem(int32 QuickIndex) const;

    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    bool MoveSlot(int32 SourceIndex, int32 DestIndex);

    // 해당 슬롯 아이템의 "인벤토리 전체 총수량" 반환
    UFUNCTION(BlueprintPure, Category = "QuickSlot")
    int32 GetTotalCountForSlot(int32 QuickIndex) const;

    UFUNCTION(BlueprintPure, Category = "QuickSlot")
    bool IsSlotAssigned(int32 QuickIndex) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot|Filter")
    bool bAllowConsumables = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot|Filter")
    bool bAllowQuestItems = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot|Filter")
    bool bAllowSkills = true;

    // 내부 체크함수
    bool IsAllowedForQuickslot(const UInventoryItem* Item) const;

    // 추가: 스킬 배정 / 해제 / 조회
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Skill")
    void AssignSkillToSlot(int32 QuickIndex, FName SkillId);

    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Skill")
    void ClearSkillFromSlot(int32 QuickIndex);

    UFUNCTION(BlueprintPure, Category = "QuickSlot|Skill")
    FName GetSkillInSlot(int32 QuickIndex) const;


protected:
    virtual void BeginPlay() override;

    UInventoryItem* FindItemByInstance(UInventoryComponent* Inv, const FGuid& InstanceId) const;

    // 인벤토리 변경을 감지해 고스트/재바인딩 반영
    void EnsureInventoryObserved(UInventoryComponent* Inv);
    UFUNCTION() void OnInventorySlotUpdated(int32 UpdatedIndex, UInventoryItem* UpdatedItem);
    UFUNCTION() void OnInventoryRefreshed();

private:
    // 관찰 중인 인벤토리 집합(중복 바인딩 방지)
    UPROPERTY() TSet<TWeakObjectPtr<UInventoryComponent>> ObservedInventories;

    TSet<TWeakObjectPtr<UInventoryComponent>> Observed;

};
