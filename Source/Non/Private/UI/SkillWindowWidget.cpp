#include "UI/SkillWindowWidget.h"
#include "UI/SkillSlotWidget.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Skill/SkillManagerComponent.h"



void USkillWindowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!SkillMgr)
        if (APawn* P = GetOwningPlayerPawn())
            SkillMgr = P->FindComponentByClass<USkillManagerComponent>();

    if (SkillMgr)
    {
        SkillMgr->OnSkillPointsChanged.AddUniqueDynamic(this, &USkillWindowWidget::OnSkillPointsChanged);
        SkillMgr->OnSkillLevelChanged.AddUniqueDynamic(this, &USkillWindowWidget::OnSkillLevelChanged);
        SkillMgr->OnJobChanged.AddUniqueDynamic(this, &USkillWindowWidget::OnJobChanged);
    }


}

void USkillWindowWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // 에디터에서 미리보기용: DataAsset 이 비어 있으면 Default 사용
    if (!DataAsset && DefaultDataAsset)
    {
        DataAsset = DefaultDataAsset;
    }
}

void USkillWindowWidget::Init(USkillManagerComponent* InMgr, USkillDataAsset* InDataAsset)
{
    UE_LOG(LogTemp, Warning, TEXT("[SkillWindow] Init called: InMgr=%s, InDA=%s"),
        InMgr ? TEXT("VALID") : TEXT("NULL"),
        *GetNameSafe(InDataAsset));
    SkillMgr = InMgr;

    // UIManager 가 넘겨준 DA 우선 사용, 없으면 Default 사용
    if (InDataAsset)
    {
        DataAsset = InDataAsset;
    }
    else if (!DataAsset && DefaultDataAsset)
    {
        DataAsset = DefaultDataAsset;
    }

    if (!DataAsset)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SkillWindow] Init: DataAsset is null"));
        return;
    }

    // 여기서 슬롯/SlotMap 전부 다시 구성
    Rebuild();

    // 스킬포인트/버튼 상태 한 번에 갱신
    RefreshAll();
}

EJobClass USkillWindowWidget::GetJobClass() const
{
    return (SkillMgr ? SkillMgr->GetJobClass() : EJobClass::Defender);
}

void USkillWindowWidget::Rebuild()
{
    UE_LOG(LogTemp, Warning, TEXT("[SkillWindow] Rebuild: SkillMgr=%s, DA=%s"),
        SkillMgr ? TEXT("VALID") : TEXT("NULL"),
        *GetNameSafe(DataAsset));
    SlotMap.Reset();
    bBuilt = false;

    // DataAsset 없으면 DefaultDataAsset라도 잡아서 시도
    if (!DataAsset && DefaultDataAsset)
    {
        DataAsset = DefaultDataAsset;
    }

    if (!DataAsset)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SkillWindow] Rebuild: DataAsset is null"));
        return;
    }

    // SkillMgr 없어도 슬롯 구성은 가능 (AllowedClass 체크만 조건부로)
    if (bUsePlacedSlots)
    {
        BuildFromPlacedSlots();
    }

    bBuilt = true;
}

void USkillWindowWidget::BuildFromPlacedSlots()
{
    if (!WidgetTree || !DataAsset) return;

    TArray<UWidget*> All;
    WidgetTree->GetAllWidgets(All);

    for (UWidget* W : All)
    {
        if (USkillSlotWidget* SkillSlot = Cast<USkillSlotWidget>(W))
        {
            USkillDataAsset* UseDA = SkillSlot->SkillDataOverride ? SkillSlot->SkillDataOverride : DataAsset;

            if (!UseDA || SkillSlot->SkillId.IsNone())
            {
                SkillSlot->SetVisibility(ESlateVisibility::Collapsed);
                continue;
            }

            const FSkillRow* RowPtr = UseDA->Skills.Find(SkillSlot->SkillId);
            if (!RowPtr)
            {
                SkillSlot->SetVisibility(ESlateVisibility::Collapsed);
                continue;
            }

            const FSkillRow& Row = *RowPtr;

            if (SkillMgr && Row.AllowedClass != SkillMgr->GetJobClass())
            {
                SkillSlot->SetVisibility(ESlateVisibility::Collapsed);
                continue;
            }

            SkillSlot->SetVisibility(ESlateVisibility::Visible);
            SkillSlot->SetupSlot(Row, SkillMgr);
            SlotMap.Add(Row.Id, SkillSlot);
        }
    }
}

void USkillWindowWidget::RefreshAll()
{
    if (SkillMgr && Text_Points)
        Text_Points->SetText(FText::AsNumber(SkillMgr->GetSkillPoints()));

    for (auto& It : SlotMap)
        if (It.Value) It.Value->Refresh();
}

void USkillWindowWidget::OnSkillPointsChanged(int32 NewPoints)
{
    // 스킬포인트 텍스트 갱신 (이미 있으면 그대로 유지)
    if (Text_Points)
    {
        Text_Points->SetText(FText::AsNumber(NewPoints));
    }

    // 포인트가 바뀌었으니, 모든 슬롯 다시 상태 갱신
    RefreshAll();
}

void USkillWindowWidget::OnSkillLevelChanged(FName SkillId, int32 /*NewLevel*/)
{
    if (USkillSlotWidget** Found = SlotMap.Find(SkillId))
        if (USkillSlotWidget* SkillSlot = *Found)
            SkillSlot->Refresh();
}

void USkillWindowWidget::OnJobChanged(EJobClass /*NewJob*/)
{
    Rebuild();
    RefreshAll();
}
