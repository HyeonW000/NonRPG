#include "UI/Party/PartyFrameWidget.h"
#include "UI/Party/PartyMemberEntryWidget.h"
#include "Core/NonPlayerController.h"
#include "System/PartyComponent.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"

UPartyFrameWidget::UPartyFrameWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    EntryWidgetClass = UPartyMemberEntryWidget::StaticClass();
}

void UPartyFrameWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!bTreeBuilt && !VBox_MemberList)
    {
        BuildCustomWidgetTree();
    }

    // 에디터에서 설정한 초기 위치(X, Y)를 뷰포트에 즉시 적용
    if (bUseDefaultPosition)
    {
        SetPositionInViewport(DefaultViewportPosition, false);
    }

    // 기본적으로 파티가 없으면 숨김
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPartyFrameWidget::NativeDestruct()
{
    if (CachedPartyComponent.IsValid())
    {
        CachedPartyComponent->OnPartyUpdated.RemoveAll(this);
        CachedPartyComponent->OnPartyLeft.RemoveAll(this);
    }

    Super::NativeDestruct();
}

FReply UPartyFrameWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bIsDragging = true;
        DragStartMouseScreenPos = InMouseEvent.GetScreenSpacePosition();

        // 현재 위젯 좌상단의 정확한 뷰포트 내부 좌표 계산 (에디터 상단 툴바 오차 제거)
        FVector2D DummyPixel;
        USlateBlueprintLibrary::AbsoluteToViewport(this, InGeometry.GetAbsolutePosition(), DummyPixel, DragStartWidgetViewportPos);

        return FReply::Handled().CaptureMouse(TakeWidget());
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UPartyFrameWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsDragging)
    {
        float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
        if (Scale <= 0.0f)
        {
            Scale = 1.0f;
        }

        // 마우스가 이동한 실제 물리 픽셀 거리를 뷰포트 스케일로 나누어 1:1로 즉각 이동 (지연감/느린 느낌 완전 제거)
        const FVector2D MouseDelta = (InMouseEvent.GetScreenSpacePosition() - DragStartMouseScreenPos) / Scale;

        SetPositionInViewport(DragStartWidgetViewportPos + MouseDelta, false);
        return FReply::Handled();
    }

    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UPartyFrameWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bIsDragging = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UPartyFrameWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    bIsDragging = false;
    Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UPartyFrameWidget::BuildCustomWidgetTree()
{
    bTreeBuilt = true;

    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }

    // 1. 최상단 컨테이너 보더 (너비 약 220px를 담을 투명/반투명 보더)
    Border_MainFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_MainFrame"));
    Border_MainFrame->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f)); // 투명
    Border_MainFrame->SetPadding(FMargin(0.f));
    WidgetTree->RootWidget = Border_MainFrame;

    // 2. 파티원 엔트리들을 담을 리스트 컨테이너 (헤더 없이 다이렉트 배치)
    VBox_MemberList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox_MemberList"));
    Border_MainFrame->AddChild(VBox_MemberList);
}

void UPartyFrameWidget::BindPartyComponent(UPartyComponent* InPartyComp)
{
    if (!InPartyComp) return;

    if (CachedPartyComponent.IsValid())
    {
        CachedPartyComponent->OnPartyUpdated.RemoveAll(this);
        CachedPartyComponent->OnPartyLeft.RemoveAll(this);
    }

    CachedPartyComponent = InPartyComp;

    InPartyComp->OnPartyUpdated.AddUniqueDynamic(this, &UPartyFrameWidget::HandlePartyUpdated);
    InPartyComp->OnPartyLeft.AddUniqueDynamic(this, &UPartyFrameWidget::HandlePartyLeft);

    // 현재 파티 상태로 즉시 1회 초기 갱신
    RefreshPartyList(InPartyComp->GetPartyData());
}

void UPartyFrameWidget::HandlePartyUpdated(const FPartyData& NewPartyData)
{
    RefreshPartyList(NewPartyData);
}

void UPartyFrameWidget::HandlePartyLeft()
{
    if (VBox_MemberList)
    {
        VBox_MemberList->ClearChildren();
    }
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPartyFrameWidget::RefreshPartyList(const FPartyData& InPartyData)
{
    if (!bTreeBuilt && !VBox_MemberList)
    {
        BuildCustomWidgetTree();
    }

    // 2인 이상일 때만 파티 프레임을 화면에 표시
    if (!InPartyData.IsInParty() || InPartyData.GetMemberCount() <= 1)
    {
        if (VBox_MemberList)
        {
            VBox_MemberList->ClearChildren();
        }
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    if (!VBox_MemberList) return;

    // 슬롯 재사용 및 동적 추가
    const int32 NumMembers = InPartyData.Members.Num();
    const int32 ExistingChildren = VBox_MemberList->GetChildrenCount();

    // 기존 자식 위젯 수와 다르면 다시 구성
    if (ExistingChildren != NumMembers)
    {
        VBox_MemberList->ClearChildren();

        TSubclassOf<UPartyMemberEntryWidget> ClassToUse = EntryWidgetClass;
        if (!ClassToUse)
        {
            ClassToUse = UPartyMemberEntryWidget::StaticClass();
        }

        for (int32 i = 0; i < NumMembers; ++i)
        {
            UPartyMemberEntryWidget* EntryWidget = CreateWidget<UPartyMemberEntryWidget>(this, ClassToUse);
            if (EntryWidget)
            {
                if (UVerticalBoxSlot* VSlot = VBox_MemberList->AddChildToVerticalBox(EntryWidget))
                {
                    VSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f)); // 슬롯 간 간격 6px
                }
                EntryWidget->UpdateMemberInfo(InPartyData.Members[i]);
            }
        }
    }
    else
    {
        // 자식 수가 같으면 내용만 업데이트
        for (int32 i = 0; i < NumMembers; ++i)
        {
            if (UPartyMemberEntryWidget* EntryWidget = Cast<UPartyMemberEntryWidget>(VBox_MemberList->GetChildAt(i)))
            {
                EntryWidget->UpdateMemberInfo(InPartyData.Members[i]);
            }
        }
    }
}
