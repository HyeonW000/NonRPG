#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DraggableWindowBase.generated.h"

class UBPC_UIManager;
class UBorder;
class UButton;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class NON_API UDraggableWindowBase : public UUserWidget
{
    GENERATED_BODY()

public:
    UDraggableWindowBase(const FObjectInitializer& ObjectInitializer);

    // BP에서 반드시 바인딩:
    // - TitleBarArea: 타이틀바 Border (WBP_DraggableWindowBase에 존재)
    // - CloseButton : 닫기 버튼 (있으면 드래그 시작 제외 & 클릭 시 창 닫기)
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Window|Chrome")
    UBorder* TitleBarArea = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Window|Chrome")
    UButton* CloseButton = nullptr;

    // 창이 화면 밖으로 나가지 않도록 클램프할지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Drag")
    bool bClampToViewport = true;

    UFUNCTION(BlueprintCallable, Category = "Window")
    virtual void CloseWindow();

    UFUNCTION(BlueprintCallable, Category = "Window")
    virtual void BringToFront();

    /** 창 제목 (BP에서 디폴트/런타임 변경 가능, CreateWidget 노드에서도 설정 가능) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Title", meta = (ExposeOnSpawn = "true"))
    FText Title = FText::FromString(TEXT("Window"));

    /** UMG에서 TitleBar 안에 배치된 TextBlock를 이름 'TitleText'로 바인딩(선택) */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Window|Title")
    UTextBlock* TitleText = nullptr;

    /** 제목을 런타임에 바꿀 때 사용 (TitleText가 있으면 자동 반영) */
    UFUNCTION(BlueprintCallable, Category = "Window|Title")
    void SetTitle(const FText& InTitle);

    /** 제목 변경을 BP에서 커스텀 처리하고 싶을 때(TitleText가 없을 때 호출됨) */
    UFUNCTION(BlueprintImplementableEvent, Category = "Window|Title")
    void BP_OnTitleChanged(const FText& NewTitle);

protected:
    virtual void NativeConstruct() override;

    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

    // 드래그 시작/종료 (이동은 Tick에서 추적)
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void   NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION()
    void OnCloseClicked();
    UFUNCTION()
    void OnClosePressed();

private:
    // 타이틀바 위인가? (단, CloseButton 영역은 제외)
    bool IsOnTitleBarExcludingClose(const FPointerEvent& E) const;

    // 화면 좌표 → 뷰포트 좌표
    static bool ToViewportPos(UWorld* World, const FVector2D& ScreenPos, FVector2D& OutViewportPos);

    // 뷰포트 내로 위치 클램프
    FVector2D ClampToViewportIfNeeded(const FVector2D& InPos, const FVector2D& DesiredSize) const;

private:
    TWeakObjectPtr<UBPC_UIManager> UIManager;

    bool bDragging = false;

    bool IsOnCloseButton(const FPointerEvent& E) const;

    // 드래그 시작 시점 저장(뷰포트 좌표)
    FVector2D DragStartWindowViewport = FVector2D::ZeroVector;
    FVector2D DragStartMouseViewport = FVector2D::ZeroVector;
};
