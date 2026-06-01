#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DraggableWindowBase.generated.h"

class UNonUIManagerComponent;
class UBorder;
class UButton;
class UTextBlock;
class UImage;
class UTexture2D;

UCLASS(Abstract, Blueprintable)
class NON_API UDraggableWindowBase : public UUserWidget
{
    GENERATED_BODY()

public:

    // 기본 처음 열릴 때 위치
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

    /** [New] 스폰 시 혹은 에디터에서 바로 꽂아줄 수 있는 윈도우 타이틀 아이콘용 텍스처 에셋 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Title", meta = (ExposeOnSpawn = "true"))
    TObjectPtr<UTexture2D> TitleIconTexture = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Window|Title")
    UTextBlock* TitleText = nullptr;

    /** [New] WBP 디자이너 상에서 'TitleIcon' 이라는 이름으로 이미지 위젯을 생성하면 C++ 변수에 자동 바인딩됩니다. */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Window|Title")
    TObjectPtr<UImage> TitleIcon = nullptr;

    UFUNCTION(BlueprintCallable, Category = "Window|Title")
    void SetTitle(const FText& InTitle);

    /** [New] 런타임에 동적으로 타이틀 아이콘을 교체할 때 호출하는 편리한 함수 */
    UFUNCTION(BlueprintCallable, Category = "Window|Title")
    void SetTitleIcon(UTexture2D* InTexture);

    UFUNCTION(BlueprintImplementableEvent, Category = "Window|Title")
    void BP_OnTitleChanged(const FText& NewTitle);

    // ----- 위치 저장/복원용 공개 함수 -----
    UFUNCTION(BlueprintCallable, Category = "Window|Position")
    void SetSavedViewportPos(const FVector2D& InPos);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Window|Position")
    bool HasSavedViewportPos() const { return bHasSavedViewportPos; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Window|Position")
    FVector2D GetSavedViewportPos() const
    {
        return bHasSavedViewportPos ? SavedViewportPos : DefaultViewportPos;
    }

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
    FVector2D ClampToViewportIfNeeded(const FVector2D& InPos) const;

private:
    TWeakObjectPtr<UNonUIManagerComponent> UIManager;

    bool bDragging = false;

    bool IsOnCloseButton(const FPointerEvent& E) const;

    // 드래그 시작 시점 기준 위치 (픽셀)
    FVector2D DragStartWindowViewport = FVector2D::ZeroVector;
    FVector2D DragStartMouseViewport = FVector2D::ZeroVector;

    // 마지막 창 위치 저장 (픽셀 좌표)
    bool bHasSavedViewportPos = false;
    FVector2D SavedViewportPos = FVector2D::ZeroVector;
};