#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameOverWidget.generated.h"

class UButton;

/**
 * 캐릭터가 사망했을 때 표시되는 게임 오버 및 리스폰 선택 위젯
 */
UCLASS()
class NON_API UNonGameOverWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    // 제자리 부활 버튼 클릭 콜백
    UFUNCTION()
    void OnRespawnInPlaceClicked();

    // 근처 부활지점 부활 버튼 클릭 콜백
    UFUNCTION()
    void OnRespawnAtNearestClicked();

    UPROPERTY(meta = (BindWidget))
    UButton* Btn_RespawnInPlace;

    UPROPERTY(meta = (BindWidget))
    UButton* Btn_RespawnAtNearest;
};
