#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Skill/SkillTypes.h"
#include "SkillSlotWidget.generated.h"


class UTextBlock;
class UButton;
class UImage;
class UBorder;
class USizeBox;
class USkillManagerComponent;
class USkillDataAsset;

/** 단일 스킬 슬롯 (에디터 배치형) */
UCLASS()
class USkillSlotWidget : public UUserWidget {
  GENERATED_BODY()
public:
  /** 슬롯 데이터/매니저 주입 */
  void SetupSlot(const FSkillRow &InRow, USkillManagerComponent *InMgr);

  /** 표시 갱신 */
  UFUNCTION(BlueprintCallable) void Refresh();

  /** 에디터에서 선택할 스킬 ID (드롭다운) */
  UPROPERTY(EditInstanceOnly, Category = "Skill",
            meta = (GetOptions = "GetSkillIdOptions"))
  FName SkillId;

  /** (옵션) 이 슬롯만 다른 DataAsset 사용 */
  UPROPERTY(EditInstanceOnly, Category = "Skill")
  USkillDataAsset *SkillDataOverride = nullptr;

  const float DragSize = 64.f;

protected:
  virtual void NativeOnInitialized() override;

  /** 드롭다운 옵션 제공 */
  UFUNCTION() TArray<FName> GetSkillIdOptions() const;

  UFUNCTION() void OnClicked_LevelUp();
  UFUNCTION() void OnIconLoaded();

  // ===== 바인딩 =====
  // ===== 바인딩 =====
  UPROPERTY(meta = (BindWidget)) UTextBlock *Text_Level = nullptr;
  UPROPERTY(meta = (BindWidget)) UButton *Btn_LevelUp = nullptr;
  UPROPERTY(meta = (BindWidgetOptional)) UBorder *LockOverlay = nullptr;
  UPROPERTY(meta = (BindWidgetOptional)) UImage *IconImage = nullptr;
  UPROPERTY(meta = (BindWidgetOptional)) UImage *ImgCooldownRadial = nullptr;
  UPROPERTY(meta = (BindWidgetOptional)) UTextBlock *TxtCooldown = nullptr;

  UPROPERTY() UMaterialInstanceDynamic *CooldownMID = nullptr;

  // ===== 데이터 =====
  UPROPERTY() USkillManagerComponent *SkillMgr = nullptr;
  UPROPERTY() FSkillRow Row;

  // Cooldown Logic
  bool bCooldownActive = false;
  float CooldownEndTime = 0.f;
  float CooldownTotal = 0.f;
  FTimerHandle CooldownTimerHandle;

  UFUNCTION()
  void OnSkillCooldownStarted(FName InSkillId, float Duration, float EndTime);
  void StartCooldown(float Duration, float EndTime);
  void ClearCooldownUI();
  void UpdateCooldownTick();

  // Drag Drop 및 Mouse Button 처리
  virtual FReply
  NativeOnMouseButtonDown(const FGeometry &InGeometry,
                          const FPointerEvent &InMouseEvent) override;
  virtual void NativeOnDragDetected(const FGeometry &InGeometry,
                                    const FPointerEvent &InMouseEvent,
                                    UDragDropOperation *&OutOperation) override;
};
