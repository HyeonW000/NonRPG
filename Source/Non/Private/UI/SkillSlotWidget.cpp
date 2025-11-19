#include "UI/SkillSlotWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "UI/SkillWindowWidget.h"
#include "Skill/SkillManagerComponent.h"

TArray<FName> USkillSlotWidget::GetSkillIdOptions() const
{
    // 1) 슬롯 오버라이드 DA 우선
    if (SkillDataOverride && SkillDataOverride->Skills.Num() > 0)
    {
        TArray<FName> Keys;
        SkillDataOverride->Skills.GenerateKeyArray(Keys);
        Keys.Sort(FNameLexicalLess());
        return Keys;
    }

    // 2) 부모 페이지의 DA 사용
    if (const USkillWindowWidget* Page = GetTypedOuter<USkillWindowWidget>())
    {
        if (const USkillDataAsset* DA = Page->GetDataAsset())
        {
            TArray<FName> Keys;
            DA->Skills.GenerateKeyArray(Keys);
            Keys.Sort(FNameLexicalLess());
            return Keys;
        }
    }
    return {};
}

void USkillSlotWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (Btn_LevelUp)
    {
        Btn_LevelUp->OnClicked.Clear();
        Btn_LevelUp->OnClicked.AddDynamic(this, &USkillSlotWidget::OnClicked_LevelUp);
    }
}

void USkillSlotWidget::SetupSlot(const FSkillRow& InRow, USkillManagerComponent* InMgr)
{
    Row = InRow;
    SkillMgr = InMgr;
    Refresh();
}

void USkillSlotWidget::Refresh()
{
    if (!SkillMgr) return;

    const int32 Lvl = SkillMgr->GetSkillLevel(Row.Id);

    if (Text_Name)  Text_Name->SetText(Row.Name);
    if (Text_Level) Text_Level->SetText(FText::FromString(FString::Printf(TEXT("Lv %d / %d"), Lvl, Row.MaxLevel)));

    // 레벨업 가능 여부
    FString Why;
    const bool bCan = SkillMgr->CanLevelUp(Row.Id, Why);
    if (Btn_LevelUp) Btn_LevelUp->SetIsEnabled(bCan);

    // 잠금 오버레이
    const bool bLocked = !bCan && (Lvl == 0);
    if (LockOverlay) LockOverlay->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

    // 아이콘
    if (IconBox)
    {
        IconBox->SetWidthOverride(IconPixelSize);
        IconBox->SetHeightOverride(IconPixelSize);
    }

    if (IconImage)
    {
        if (Row.Icon.IsNull())
        {
            IconImage->SetBrush(FSlateBrush());
        }
        else
        {
            if (UTexture2D* Tex = Row.Icon.Get())
            {
                IconImage->SetBrushFromTexture(Tex, /*bMatchSize=*/false);
            }
            else if (UTexture2D* TexSync = Row.Icon.LoadSynchronous())
            {
                IconImage->SetBrushFromTexture(TexSync, /*bMatchSize=*/false);
            }
            else
            {
                FStreamableManager& SM = UAssetManager::GetStreamableManager();
                SM.RequestAsyncLoad(
                    Row.Icon.ToSoftObjectPath(),
                    FStreamableDelegate::CreateUObject(this, &USkillSlotWidget::OnIconLoaded)
                );
            }
        }
    }
}

void USkillSlotWidget::OnClicked_LevelUp()
{
    if (!SkillMgr) return;
    SkillMgr->TryLearnOrLevelUp(Row.Id);
    Refresh(); // 즉시 갱신(서버 복제 도착 시 델리게이트로 다시 갱신됨)
}

void USkillSlotWidget::OnIconLoaded()
{
    if (!IconImage) return;
    if (UTexture2D* Tex = Row.Icon.Get())
    {
        IconImage->SetBrushFromTexture(Tex, /*bMatchSize=*/false);
    }
}
