#include "UI/Skill/SkillWindowWidget.h"
#include "UI/Skill/SkillSlotWidget.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h" // [New]
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
    SkillMgr = InMgr;

    // UIManager 가 넘겨준 DA 우선 사용, 없으면 Default 사용
    bool bUsingFallback = false;
    if (InDataAsset)
    {
        DataAsset = InDataAsset;
    }
    // [New] UIManager 설정이 비었어도, SkillManager가 알고 있다면 그것을 사용 (Fallback)
    else if (SkillMgr && SkillMgr->GetDataAsset())
    {
        DataAsset = SkillMgr->GetDataAsset();
        bUsingFallback = true;
    }
    else if (!DataAsset && DefaultDataAsset)
    {
        DataAsset = DefaultDataAsset;
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
    SlotMap.Reset();
    bBuilt = false;

    // DataAsset 없으면 DefaultDataAsset라도 잡아서 시도
    if (!DataAsset && DefaultDataAsset)
    {
        DataAsset = DefaultDataAsset;
    }

    if (!DataAsset)
    {
        return;
    }

    // SkillMgr 없어도 슬롯 구성은 가능 (AllowedClass 체크만 조건부로)
    if (bUsePlacedSlots)
    {
        BuildFromPlacedSlots();
    }

    // [New] 스위처가 있다면 직업에 맞게 페이지 전환
    if (ClassSwitcher && SkillMgr)
    {
        int32 JobIndex = (int32)SkillMgr->GetJobClass();
        int32 NumPages = ClassSwitcher->GetNumWidgets();
        
        if (JobIndex >= 0 && JobIndex < NumPages)
        {
            ClassSwitcher->SetActiveWidgetIndex(JobIndex);
        }
    }
    
    bBuilt = true;
}

void USkillWindowWidget::BuildFromPlacedSlots()
{
    if (!WidgetTree || !DataAsset) return;

    // [Changed] 재귀적으로 모든 슬롯 찾기
    TArray<USkillSlotWidget*> FoundSlots;
    FindSlotsRecursively(this, FoundSlots);

    for (USkillSlotWidget* SkillSlot : FoundSlots)
    {
        USkillDataAsset* UseDA = SkillSlot->SkillDataOverride ? SkillSlot->SkillDataOverride : DataAsset;

        if (!UseDA || SkillSlot->SkillId.IsNone())
        {
            // SkillSlot->SetVisibility(ESlateVisibility::Collapsed); // (페이지 자체를 숨길 필요는 없음)
            continue;
        }

        const FSkillRow* RowPtr = UseDA->Skills.Find(SkillSlot->SkillId);
        if (!RowPtr) continue;

        const FSkillRow& Row = *RowPtr;

        // "직업 맞지 않으면 숨김" 로직은, 스위처 방식에선 페이지 단위로 이미 가려지므로 필수는 아님.
        // 하지만 단일 페이지 방식도 지원하기 위해 유지.
        if (SkillMgr && Row.AllowedClass != SkillMgr->GetJobClass())
        {
             // 로그로 스킵 원인 파악
             // UE_LOG(LogTemp, Verbose, TEXT("Skip Slot %s: Allowed=%d vs MyJob=%d"), *SkillSlot->SkillId.ToString(), (int32)Row.AllowedClass, (int32)SkillMgr->GetJobClass());
        }

        SkillSlot->SetVisibility(ESlateVisibility::Visible);
        SkillSlot->SetupSlot(Row, SkillMgr);
        SlotMap.Add(Row.Id, SkillSlot);
    }
}

void USkillWindowWidget::FindSlotsRecursively(UUserWidget* Root, TArray<USkillSlotWidget*>& OutSlots)
{
    if (!Root || !Root->WidgetTree) return;

    TArray<UWidget*> All;
    Root->WidgetTree->GetAllWidgets(All);

    for (UWidget* W : All)
    {
        if (USkillSlotWidget* FoundSlot = Cast<USkillSlotWidget>(W))
        {
            OutSlots.Add(FoundSlot);
        }
        else if (UUserWidget* ChildUW = Cast<UUserWidget>(W))
        {
            // 자식 WBP 내부도 탐색 (예: WBP_Skill_Berserker 안에 있는 슬롯들)
            FindSlotsRecursively(ChildUW, OutSlots);
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
    // [Fix] 직업이 바뀌면 DataAsset도 그에 맞춰서 갱신해야 함!
    if (SkillMgr && SkillMgr->GetDataAsset())
    {
        DataAsset = SkillMgr->GetDataAsset();
    }

    Rebuild();
    RefreshAll();
}
