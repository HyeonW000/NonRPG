#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NonTitleWidget.generated.h"

/**
 * 타이틀 화면 (아무 키나 누르세요 -> 메인 메뉴)
 */
UCLASS()
class NON_API UNonTitleWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;

protected:
	// === UI 바인딩 ===
	
	// 1. 초기 화면: 아무 키나 누르세요 버튼 (전체 화면 투명 버튼)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UButton> Btn_AnyKey;

	// 2. 메뉴 컨테이너 (처음엔 숨김 or 애니메이션으로 등장)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidget> MenuContainer;

	// 3. 메뉴 버튼들
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UButton> Btn_Play;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UButton> Btn_Option;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UButton> Btn_Exit;

	// 4. 등장 애니메이션 (선택 사항)
	UPROPERTY(meta = (BindWidgetAnim), Transient)
	TObjectPtr<class UWidgetAnimation> Anim_ShowMenu;

private:
	UFUNCTION() void OnAnyKeyClicked();
	UFUNCTION() void OnPlayClicked();
	UFUNCTION() void OnOptionClicked();
	UFUNCTION() void OnExitClicked();
};
