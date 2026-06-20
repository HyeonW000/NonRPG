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
    UDraggableWindowBase(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Window|Chrome")
    UBorder* TitleBarArea = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Window|Chrome")
    UButton* CloseButton = nullptr;

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

    /** 처음 창이 생성되어 열릴 때 배치될 기본 뷰포트 좌표 (마이너스 값일 경우 에디터 디자인 배치 위치를 따름) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window|Layout")
    FVector2D DefaultWindowPos = FVector2D(-1.f, -1.f);

    UFUNCTION(BlueprintCallable, Category = "Window|Title")
    void SetTitle(const FText& InTitle);

    /** [New] 런타임에 동적으로 타이틀 아이콘을 교체할 때 호출하는 편리한 함수 */
    UFUNCTION(BlueprintCallable, Category = "Window|Title")
    void SetTitleIcon(UTexture2D* InTexture);

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

private:
    TWeakObjectPtr<UNonUIManagerComponent> UIManager;

    bool IsOnCloseButton(const FPointerEvent& E) const;

    // 심플 드래그용 상태 변수
    bool bDragging = false;
    FVector2D DragStartMousePos = FVector2D::ZeroVector;
    FVector2D DragStartWindowPos = FVector2D::ZeroVector;
    FVector2D LastWindowPos = FVector2D(-1.f, -1.f);
};