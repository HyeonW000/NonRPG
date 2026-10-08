#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "System/PartyTypes.h"
#include "PartyFrameWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UBorder;
class UPartyComponent;
class UPartyMemberEntryWidget;

/**
 * 화면 좌측에 파티원 목록을 나열하는 메인 파티 프레임 위젯
 */
UCLASS()
class NON_API UPartyFrameWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPartyFrameWidget(const FObjectInitializer& ObjectInitializer);

    /** 파티 컴포넌트와 바인딩하여 자동 갱신 청취 */
    void BindPartyComponent(UPartyComponent* InPartyComp);

    /** 파티 데이터에 맞춰 UI 전체 갱신 */
    void RefreshPartyList(const FPartyData& InPartyData);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // 마우스 드래그 이동 지원
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

    /** C++ 코드 기반 위젯 트리 동적 구축 */
    void BuildCustomWidgetTree();

    UFUNCTION()
    void HandlePartyUpdated(const FPartyData& NewPartyData);

    UFUNCTION()
    void HandlePartyLeft();

public:
    UPROPERTY(meta = (BindWidgetOptional))
    UBorder* Border_MainFrame = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UVerticalBox* VBox_MemberList = nullptr;

    /** 파티원 슬롯 위젯 클래스 (미지정 시 UPartyMemberEntryWidget 기본 사용) */
    UPROPERTY(EditDefaultsOnly, Category = "Party")
    TSubclassOf<UPartyMemberEntryWidget> EntryWidgetClass;

    /** 게임 시작 시 배치될 뷰포트 초기 위치 (X, Y 좌표 - 에디터에서 자유롭게 수정 가능) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Party|Layout")
    FVector2D DefaultViewportPosition = FVector2D(30.f, 220.f);

    /** 초기 위치 자동 적용 여부 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Party|Layout")
    bool bUseDefaultPosition = true;

private:
    TWeakObjectPtr<UPartyComponent> CachedPartyComponent = nullptr;
    bool bTreeBuilt = false;

    /** 마우스 드래그 이동 관련 상태 */
    bool bIsDragging = false;
    FVector2D DragStartMouseScreenPos = FVector2D::ZeroVector;
    FVector2D DragStartWidgetViewportPos = FVector2D::ZeroVector;
};
