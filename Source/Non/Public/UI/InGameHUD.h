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

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateCharacterName(const FString& NewName);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateClassIcon(class UTexture2D* NewIcon);

    // [New] 타겟 정보 갱신 (Tick에서 호출 권장)
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateTargetInfo(bool bShow, const FString& Name, float HP, float MaxHP, float Distance);

    // [New] 보스 정보 토글 및 갱신
    UFUNCTION(BlueprintCallable, Category = "HUD|Boss")
    void ShowBossFrame(bool bShow, const FString& BossName = TEXT(""), float HP = 0.f, float MaxHP = 1.f);

    UFUNCTION(BlueprintCallable, Category = "HUD|Boss")
    void UpdateBossHP(float HP, float MaxHP);

    // ── [New] 캐스팅 바 ──────────────────────────────────────────

    /** GA_GroundTarget 에서 직접 호출. 캐스팅 바를 보이게 하고 타이머 시작. */
    UFUNCTION(BlueprintCallable, Category = "HUD|Casting")
    void StartCasting(float Duration);

    /** 캐스팅 완료 또는 캔슬 시 호출. 바 숨기기. */
    UFUNCTION(BlueprintCallable, Category = "HUD|Casting")
    void StopCasting();

    /** 현재 캐스팅 중인지 */
    UFUNCTION(BlueprintPure, Category = "HUD|Casting")
    bool IsCasting() const { return bIsCasting; }

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

    // Character Name
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_CharacterName = nullptr;

    // Class Icon
    UPROPERTY(meta = (BindWidgetOptional))
    class UImage* Image_ClassIcon = nullptr;

    /* ===== [New] Target Frame (적 정보) ===== */
    UPROPERTY(meta = (BindWidgetOptional))
    class UOverlay* Overlay_TargetFrame = nullptr; // 전체 컨테이너

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_TargetName = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* ProgressBar_TargetHP = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_TargetDistance = nullptr;

    /* ===== [New] Boss Frame (보스 전용 UI) ===== */
    UPROPERTY(meta = (BindWidgetOptional))
    class UOverlay* Overlay_BossFrame = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_BossName = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* ProgressBar_BossHP = nullptr;

    /* ===== [New] 캐스팅 바 ===== */

    /** WBP 디자이너에서 이름을 'Overlay_CastingBar' 로 맞춰 두세요 */
    UPROPERTY(meta = (BindWidgetOptional))
    class UOverlay* Overlay_CastingBar = nullptr;

    /** WBP 디자이너에서 이름을 'ProgressBar_CastingBar' 로 맞춰 두세요 */
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* ProgressBar_CastingBar = nullptr;

    // 비절 Casting 실시간 상태
    bool  bIsCasting        = false;
    float CastTotalDuration = 1.f;
    float CastElapsed       = 0.f;

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

    // Boss HP
    float BossHP_Current = 0.f;
    float BossHP_Max = 1.f;
    float BossHP_DisplayPercent = 0.f;

    // Level
    int32 CurrentLevel = 1;

    /** 바 보간 속도 (전부 공통) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD", meta = (AllowPrivateAccess = "true"))
    float BarLerpSpeed = 6.f;
};
