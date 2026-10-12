#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingScreenWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;

/**
 * 맵/던전 이동 시 배경 이미지, 랜덤 팁, 로딩 진행 게이지를 제어하는 위젯 클래스
 */
UCLASS(Blueprintable, BlueprintType)
class NON_API ULoadingScreenWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    ULoadingScreenWidget(const FObjectInitializer& ObjectInitializer);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** 팁 문구를 랜덤으로 선택하여 텍스트 블록에 출력 */
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void RefreshTipText();

public:
    // ── UMG 바인딩 위젯 (Blueprint WBP에서 이름만 일치시키면 C++이 자동 조작) ──
    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Loading|Widgets")
    TObjectPtr<UTextBlock> Text_Tip;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Loading|Widgets")
    TObjectPtr<UProgressBar> ProgressBar_Loading;

    UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Loading|Widgets")
    TObjectPtr<UImage> Image_Background;

    // ── 설정 프로퍼티 (에디터 디테일 패널에서 수정 가능) ──
    /** 화면에 랜덤으로 표시될 게임 꿀팁 목록 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading|Settings")
    TArray<FString> TipList;

    /** 로딩 게이지가 0에서 100%까지 차오르는 목표 시간 (초) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading|Settings", meta = (ClampMin = "0.5"))
    float FillDuration = 2.0f;

    /** 새 레벨에 캐릭터가 스폰되었을 때 호출하여 90% -> 100%로 마무리를 채움 */
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void FinishLoading(float InFinishDuration = 0.5f);

    UFUNCTION(BlueprintPure, Category = "Loading")
    bool IsFinishing() const { return bIsFinishing; }

private:
    /** 현재 진행 퍼센트 (0.0 ~ 1.0) */
    float CurrentProgress = 0.0f;

    /** 마무리 100% 채우기 진행 중인지 여부 */
    bool bIsFinishing = false;

    /** 마무리 채우기에 걸리는 시간 */
    float FinishDuration = 0.5f;

    /** 마무리 시작 당시의 게이지 퍼센트 */
    float ProgressAtFinishStart = 0.0f;

    /** 마무리 진행 경과 시간 */
    float FinishElapsed = 0.0f;
};
