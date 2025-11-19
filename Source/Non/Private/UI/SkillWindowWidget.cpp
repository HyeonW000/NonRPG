#include "UI/SkillWindowWidget.h"
#include "UI/SkillSlotWidget.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Skill/SkillManagerComponent.h"

void USkillWindowWidget::Init(USkillManagerComponent* InMgr, USkillDataAsset* InData)
{
    SkillMgr = InMgr;
    DataAsset = InData;

    if (SkillMgr)
    {
        SkillMgr->OnSkillPointsChanged.RemoveAll(this);
        SkillMgr->OnSkillLevelChanged.RemoveAll(this);
        SkillMgr->OnJobChanged.RemoveAll(this);

        SkillMgr->OnSkillPointsChanged.AddUniqueDynamic(this, &USkillWindowWidget::OnSkillPointsChanged);
        SkillMgr->OnSkillLevelChanged.AddUniqueDynamic(this, &USkillWindowWidget::OnSkillLevelChanged);
        SkillMgr->OnJobChanged.AddUniqueDynamic(this, &USkillWindowWidget::OnJobChanged);
    }

    Rebuild();
    RefreshAll();
}

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

    Rebuild();
    RefreshAll();
}

EJobClass USkillWindowWidget::GetJobClass() const
{
    return (SkillMgr ? SkillMgr->GetJobClass() : EJobClass::Defender);
}

void USkillWindowWidget::Rebuild()
{
    SlotMap.Reset();
    bBuilt = false;

    if (!SkillMgr || !DataAsset) return;

    if (bUsePlacedSlots) BuildFromPlacedSlots();

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
    if (Text_Points) Text_Points->SetText(FText::AsNumber(NewPoints));
    for (auto& It : SlotMap)
        if (It.Value) It.Value->Refresh();
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
