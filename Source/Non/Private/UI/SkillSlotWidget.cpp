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
    const bool bHasMgr = (SkillMgr != nullptr);
    int32 Lvl = 0;
    bool bCanLevelUp = false;
    bool bIsMaxLevel = false;
    int32 CurPoints = 0;

    if (bHasMgr)
    {
        // 현재 스킬 레벨
        Lvl = SkillMgr->GetSkillLevel(Row.Id);
        bIsMaxLevel = (Lvl >= Row.MaxLevel);
        CurPoints = SkillMgr->GetSkillPoints();

        // 레벨 텍스트

        if (Text_Level)
        {
            Text_Level->SetText(
                FText::FromString(FString::Printf(TEXT("Lv %d / %d"), Lvl, Row.MaxLevel)));
        }

        // 레벨업 가능 여부(조건 체크: 직업, MaxLevel, 포인트 등)
        FString Why;
        bCanLevelUp = SkillMgr->CanLevelUp(Row.Id, Why);

        if (Btn_LevelUp)
        {
            // 버튼이 "보여야" 하는 조건:
            //  - 스킬포인트 > 0
            //  - 아직 Max 레벨 아님
            const bool bShouldShowButton = (CurPoints > 0) && !bIsMaxLevel;

            Btn_LevelUp->SetVisibility(
                bShouldShowButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

            // 보이는 경우에만 Enable/Disable 적용
            if (bShouldShowButton)
            {
                Btn_LevelUp->SetIsEnabled(bCanLevelUp);
            }
            else
            {
                Btn_LevelUp->SetIsEnabled(false);
            }
        }
    }
    else
    {
        // 매니저 없을 때 기본 표시
        if (Text_Level)
        {
            Text_Level->SetText(
                FText::FromString(FString::Printf(TEXT("Lv %d / %d"), 0, Row.MaxLevel)));
        }

        if (Btn_LevelUp)
        {
            Btn_LevelUp->SetIsEnabled(false);
            Btn_LevelUp->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // 잠금 오버레이 (레벨 0이고 지금 찍을 수 없는 상태면 잠금)
    const bool bLocked = !bCanLevelUp && (Lvl == 0);
    if (LockOverlay)
    {
        LockOverlay->SetVisibility(
            bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    // === 아이콘 ===

    if (IconImage)
    {
        if (Row.Icon.IsNull())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[SkillSlot] Row %s has NO Icon"), *Row.Id.ToString());

            IconImage->SetBrush(FSlateBrush());
        }
        else
        {
            UE_LOG(LogTemp, Log,
                TEXT("[SkillSlot] Row %s Icon = %s"),
                *Row.Id.ToString(), *Row.Icon.ToString());

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

    const FName EffectiveId = !SkillId.IsNone() ? SkillId : Row.Id;
    if (EffectiveId.IsNone()) return;

    SkillMgr->TryLearnOrLevelUp(EffectiveId);

    // 이 슬롯 즉시 갱신(서버/포인트 변경은 OnSkillPointsChanged로 전체 Refresh)
    Refresh();
}

void USkillSlotWidget::OnIconLoaded()
{
    if (!IconImage) return;
    if (UTexture2D* Tex = Row.Icon.Get())
    {
        IconImage->SetBrushFromTexture(Tex, /*bMatchSize=*/false);
    }
}
