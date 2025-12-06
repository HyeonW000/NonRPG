#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InGameHUD.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class NON_API UInGameHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    UInGameHUD(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
    /** ASC/UIManager 쪽에서 호출할 함수들 */
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateHP(float Current, float Max);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateMP(float Current, float Max);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateSP(float Current, float Max);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateEXP(float Current, float Max);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateLevel(int32 NewLevel);

protected:
    /* ===== UMG 위젯 바인딩 ===== */

    // HP
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* ProgressBar_HP = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_HP = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_HPAmount = nullptr;

    // MP
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* ProgressBar_MP = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_MP = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_MPAmount = nullptr;

    // SP
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* ProgressBar_SP = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_SP = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_SPAmount = nullptr;

    // EXP
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* ProgressBar_EXP = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_EXP = nullptr;

    // Level
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_Level = nullptr;

    /* ===== 내부 값 (Target / Display) ===== */

    // HP
    float HP_Current = 0.f;
    float HP_Max = 1.f;
    float HP_DisplayPercent = 0.f;

    // MP
    float MP_Current = 0.f;
    float MP_Max = 1.f;
    float MP_DisplayPercent = 0.f;

    // SP
    float SP_Current = 0.f;
    float SP_Max = 1.f;
    float SP_DisplayPercent = 0.f;

    // EXP
    float EXP_Current = 0.f;
    float EXP_Max = 1.f;
    float EXP_DisplayPercent = 0.f;

    // Level
    int32 CurrentLevel = 1;

    /** 바 보간 속도 (전부 공통) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD", meta = (AllowPrivateAccess = "true"))
    float BarLerpSpeed = 6.f;
};
