#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuickSlotSlotWidget.generated.h"

class UQuickSlotManager;
class UQuickSlotBarWidget;
class UInventoryComponent;
class UInventoryItem;
class UItemDragDropOperation;
class UImage;
class UTextBlock;
class USkillDragDropOperation;

UCLASS()
class NON_API UQuickSlotSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot", meta = (ClampMin = "0", ClampMax = "9"))
    int32 QuickIndex = 0;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "QuickSlot|UI")
    UImage* IconImage = nullptr;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "QuickSlot|UI")
    UTextBlock* CountText = nullptr;

    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "QuickSlot")
    void UpdateVisualBP(UInventoryItem* Item);

    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    void UpdateVisual(UInventoryItem* Item);

    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    void Refresh();

    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    void SetManager(UQuickSlotManager* InManager);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot")
    TSubclassOf<UUserWidget> DragVisualClass = nullptr;

    FName GetAssignedSkillId() const { return AssignedSkillId; }

    void SetAssignedSkillId(FName NewId);

    void ClearSkillAssignment();

    // 쿨타임 시작 (QuickSlotBarWidget 에서 호출)
    void StartCooldown(float InDuration, float InEndTime);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void   NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool   NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool   NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    
    // virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override; // 삭제: Timer로 대체

    // 기존 BindWidget 들 아래에 추가
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* CooldownOverlay = nullptr;

    UPROPERTY()
    UMaterialInstanceDynamic* CooldownMID = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* CooldownText = nullptr;

private:
    TWeakObjectPtr<UQuickSlotManager> Manager;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuickSlot", meta = (AllowPrivateAccess = "true"))
    FName AssignedSkillId = NAME_None;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> CachedIcon = nullptr;

    // 쿨타임 상태
    bool  bCooldownActive = false;
    float CooldownEndTime = 0.f;
    float CooldownTotal = 0.f;

    FTimerHandle CooldownTimerHandle;

    void UpdateCooldownTick(); // 타이머함수

    void ClearCooldownUI();
    void ResyncCooldownFromSkill();
    void UpdateSkillIconFromData();
};
