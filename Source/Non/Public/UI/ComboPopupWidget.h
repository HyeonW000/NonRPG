#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ComboPopupWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * C++에서 블루프린트 위젯(WBP_ComboPopup)을 직접 조작하기 위한 부모 클래스입니다.
 */
UCLASS()
class NON_API UComboPopupWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 연계 스킬 팝업을 활성화하고 아이콘과 단축키를 띄웁니다. (쿨타임 정보 추가 수신) */
    UFUNCTION(BlueprintCallable, Category = "UI|Combo")
    void ShowCombo(TSoftObjectPtr<UTexture2D> NextIcon, float Duration, FText SlotKey, float CooldownRemaining = 0.f, float CooldownTotal = 0.f);

    /** 블루프린트에서 가드 팝업창이 나타날 때 애니메이션이나 이벤트를 손쉽게 트리거할 수 있는 징검다리 이벤트입니다. */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Combo")
    void OnShowComboTriggered();

    /** 블루프린트에서 가드 팝업창이 사라질 때 애니메이션을 트리거할 수 있는 징검다리 이벤트입니다. */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Combo")
    void OnHideComboTriggered();

    /** 블루프린트 사라짐 애니메이션이 완전히 끝난 후 가시성을 실제로 Collapsed 시키는 마무리 함수입니다. */
    UFUNCTION(BlueprintCallable, Category = "UI|Combo")
    void FinishHideCombo();

    /** 연계 스킬 팝업을 화면에서 숨깁니다. */
    UFUNCTION(BlueprintCallable, Category = "UI|Combo")
    void HideCombo();

    /** 쿨다운 오버레이 구동 시작 */
    void StartCooldown(float InDuration, float InEndTime);

    /** 쿨다운 오버레이 정리 */
    void ClearCooldownUI();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** 매 틱마다 쿨타임 남은 시간과 원형 진행률을 갱신합니다. */
    void UpdateCooldownTick();

protected:
    /** 
     * UMG 디자이너(블루프린트) 내부에 'Image_ComboIcon' 이라는 
     * 이름으로 된 이미지 위젯이 존재하면 자동으로 C++ 변수와 매핑됩니다.
     */
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* Image_ComboIcon = nullptr;

    /** 
     * UMG 디자이너(블루프린트) 내부에 'TextBlock_SlotKey' 라는 
     * 이름으로 된 텍스트 위젯이 존재하면 자동으로 C++ 변수와 매핑됩니다.
     */
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_SlotKey = nullptr;

    /** [New] 쿨다운 radial 오버레이 이미지 */
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* Image_Cooldown = nullptr;

    /** [New] 쿨다운 초 텍스트 */
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TextBlock_Cooldown = nullptr;

private:
    /** 쿨다운 타이머 핸들 */
    FTimerHandle CooldownTimerHandle;

    /** 쿨다운 활성화 정보 */
    bool bCooldownActive = false;
    float CooldownTotal = 0.f;
    float CooldownEndTime = 0.f;

    /** 쿨다운용 Dynamic Material Instance */
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> CooldownMID = nullptr;
};
