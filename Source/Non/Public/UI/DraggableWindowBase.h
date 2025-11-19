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

    // 클래스 public 혹은 protected 쪽 어딘가 적당한 곳에 추가
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Window")
    FVector2D DefaultViewportPos = FVector2D(100.f, 100.f);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Window")
    FVector2D GetDefaultViewportPos() const { return DefaultViewportPos; }

    UDraggableWindowBase(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Window|Chrome")
    UBorder* TitleBarArea = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Window|Chrome")
    UButton* CloseButton = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Drag")
    bool bClampToViewport = true;

    UFUNCTION(BlueprintCallable, Category = "Window")
    virtual void CloseWindow();

    UFUNCTION(BlueprintCallable, Category = "Window")
    virtual void BringToFront();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Title", meta = (ExposeOnSpawn = "true"))
    FText Title = FText::FromString(TEXT("Window"));

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Window|Title")
    UTextBlock* TitleText = nullptr;

    UFUNCTION(BlueprintCallable, Category = "Window|Title")
    void SetTitle(const FText& InTitle);

    UFUNCTION(BlueprintImplementableEvent, Category = "Window|Title")
    void BP_OnTitleChanged(const FText& NewTitle);

protected:
    virtual void NativeConstruct() override;

    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void   NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION()
    void OnCloseClicked();
    UFUNCTION()
    void OnClosePressed();

private:
    bool IsOnTitleBarExcludingClose(const FPointerEvent& E) const;
    static bool ToViewportPos(UWorld* World, const FVector2D& ScreenPos, FVector2D& OutViewportPos);
    FVector2D ClampToViewportIfNeeded(const FVector2D& InPos, const FVector2D& DesiredSize) const;

private:
    TWeakObjectPtr<UBPC_UIManager> UIManager;

    bool bDragging = false;

    bool IsOnCloseButton(const FPointerEvent& E) const;

    FVector2D DragStartWindowViewport = FVector2D::ZeroVector;
    FVector2D DragStartMouseViewport = FVector2D::ZeroVector;
};
