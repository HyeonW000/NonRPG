#include "UI/Skill/SkillSlotWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/Skill/SkillWindowWidget.h"
#include "UI/Skill/SkillDragDropOperation.h"
#include "Skill/SkillManagerComponent.h"
#include "UI/UIViewportUtils.h"

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
    SetIsFocusable(false); // 포커스 방지

    if (Btn_LevelUp)
    {
        Btn_LevelUp->OnClicked.Clear();
        Btn_LevelUp->OnClicked.AddDynamic(this, &USkillSlotWidget::OnClicked_LevelUp);
    }
    
    // Cooldown UI Init
    if (ImgCooldownRadial)
    {
        ImgCooldownRadial->SetVisibility(ESlateVisibility::Hidden);
        if (UObject* ResObj = ImgCooldownRadial->GetBrush().GetResourceObject())
        {
            if (UMaterialInterface* MI = Cast<UMaterialInterface>(ResObj))
            {
                CooldownMID = UMaterialInstanceDynamic::Create(MI, this);
                if (CooldownMID)
                {
                    FSlateBrush B = ImgCooldownRadial->GetBrush();
                    B.SetResourceObject(CooldownMID);
                    ImgCooldownRadial->SetBrush(B);
                    CooldownMID->SetScalarParameterValue(TEXT("Fill"), 0.f);
                }
            }
        }
    }
    if (TxtCooldown) TxtCooldown->SetVisibility(ESlateVisibility::Collapsed);
}

void USkillSlotWidget::SetupSlot(const FSkillRow& InRow, USkillManagerComponent* InMgr)
{
    // 이전 바인딩 해제
    if (SkillMgr)
    {
        SkillMgr->OnSkillCooldownStarted.RemoveDynamic(this, &USkillSlotWidget::OnSkillCooldownStarted);
    }

    Row = InRow;
    SkillMgr = InMgr;

    // 새 바인딩
    if (SkillMgr)
    {
        SkillMgr->OnSkillCooldownStarted.AddUniqueDynamic(this, &USkillSlotWidget::OnSkillCooldownStarted);
    }

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

        // 레벨업 가능 여부
        FString Why;
        bCanLevelUp = SkillMgr->CanLevelUp(Row.Id, Why);

        if (Btn_LevelUp)
        {
            const bool bShouldShowButton = (CurPoints > 0) && !bIsMaxLevel;
            Btn_LevelUp->SetVisibility(
                bShouldShowButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

            if (bShouldShowButton)
            {
                Btn_LevelUp->SetIsEnabled(bCanLevelUp);
            }
            else
            {
                Btn_LevelUp->SetIsEnabled(false);
            }
        }
        
        // --- 쿨타임 초기 상태 체크 ---
        // 이미 쿨타임 중인지 확인해서 UI 갱신
        float Remaining = 0.f;
        if (SkillMgr->IsOnCooldown(Row.Id, Remaining))
        {
             // 쿨타임 중이면, UI Timer 시작
             if (Remaining > 0.f)
             {
                 if (UWorld* World = GetWorld())
                 {
                     // MaxCooldown을 모르므로 추정이 필요하나, Row 데이터에서 가져오기 시도
                     float MaxCooldown = 0.f;
                     // Row.CoolDownTime이 있다면 사용
                     if (Row.Cooldown > 0.f) MaxCooldown = Row.Cooldown;
                     else MaxCooldown = Remaining; // Fallback

                     StartCooldown(MaxCooldown, World->GetTimeSeconds() + Remaining);
                 }
             }
        }
        else
        {
            if (bCooldownActive && Remaining <= 0.f)
            {
                ClearCooldownUI();
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
        ClearCooldownUI();
    }

    // === 잠금 오버레이 ===
    const bool bLocked = (Lvl <= 0);

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

//Drag Drop 관련
//Drag Drop 관련
FReply USkillSlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // 1. Input Mode Ensure
        if (APlayerController* PC = GetOwningPlayer())
        {
            FInputModeGameAndUI Mode;
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Mode.SetHideCursorDuringCapture(false);
            Mode.SetWidgetToFocus(nullptr);
            PC->SetInputMode(Mode);
        }

        // 2. Detect Drag
        // 좌클릭 시 드래그 감지 시작
        FEventReply ER = UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton);
        FReply Reply = ER.NativeReply;

        // 3. Force Focus back to GameViewport (Crucial for WASD continuity)
        if (TSharedPtr<SViewport> VP = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
        {
            Reply = Reply.SetUserFocus(StaticCastSharedRef<SWidget>(VP.ToSharedRef()), EFocusCause::SetDirectly);
            FSlateApplication::Get().SetKeyboardFocus(StaticCastSharedPtr<SWidget>(VP), EFocusCause::SetDirectly);
        }

        return Reply;
    }

    return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}
/*
FReply USkillSlotWidget::NativeOnMouseButtonDown(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    // Deprecated in favor of Preview + Focus logic
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
*/
void USkillSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    OutOperation = nullptr;

    if (!SkillMgr)
        return;

    const FName EffectiveId = !SkillId.IsNone() ? SkillId : Row.Id;
    if (EffectiveId.IsNone())
        return;

    const int32 CurLevel = SkillMgr->GetSkillLevel(EffectiveId);
    if (CurLevel <= 0)
        return;

    // 유효한 스킬 없으면 드래그 안 함
    if (Row.Id.IsNone())
        return;

    USkillDragDropOperation* Op = NewObject<USkillDragDropOperation>(this);
    Op->SkillId = Row.Id;
    Op->Icon = Row.Icon;

    // === DragVisual (아이콘 고스트) ===
    UTexture2D* IconTex = nullptr;
    if (!Row.Icon.IsNull())
    {
        IconTex = Row.Icon.Get();
        if (!IconTex)
        {
            IconTex = Row.Icon.LoadSynchronous();
        }
    }

    if (IconTex)
    {
        UImage* Img = NewObject<UImage>(this);
        Img->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

        FSlateBrush Brush;
        Brush.SetResourceObject(IconTex);
        Brush.ImageSize = FVector2D(DragSize, DragSize);
        Img->SetBrush(Brush);

        Op->DefaultDragVisual = Img;
        Op->Pivot = EDragPivot::MouseDown;
        Op->Offset = FVector2D::ZeroVector;
    }

    OutOperation = Op;
}
// --------------------------------------------------------------------------------------
// Cooldown Logic
// --------------------------------------------------------------------------------------
void USkillSlotWidget::OnSkillCooldownStarted(FName InSkillId, float Duration, float EndTime)
{
    const FName MySkillId = !SkillId.IsNone() ? SkillId : Row.Id;
    if (MySkillId.IsNone()) return;

    if (MySkillId == InSkillId)
    {
        StartCooldown(Duration, EndTime);
    }
}

void USkillSlotWidget::StartCooldown(float Duration, float EndTime)
{
    if (Duration <= 0.f) return;

    CooldownTotal = Duration;
    CooldownEndTime = EndTime;
    bCooldownActive = true;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(CooldownTimerHandle, this, &USkillSlotWidget::UpdateCooldownTick, 0.05f, true);
    }

    if (ImgCooldownRadial) ImgCooldownRadial->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UpdateCooldownTick();
}

void USkillSlotWidget::ClearCooldownUI()
{
    bCooldownActive = false;
    CooldownEndTime = 0.f;
    CooldownTotal = 0.f;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CooldownTimerHandle);
    }

    if (ImgCooldownRadial) ImgCooldownRadial->SetVisibility(ESlateVisibility::Hidden);
    if (TxtCooldown)       TxtCooldown->SetVisibility(ESlateVisibility::Collapsed);

    if (CooldownMID)
    {
        CooldownMID->SetScalarParameterValue(TEXT("Fill"), 0.f);
    }
}

void USkillSlotWidget::UpdateCooldownTick()
{
    if (!bCooldownActive)
    {
        ClearCooldownUI();
        return;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    const float Now = World->GetTimeSeconds();
    const float Remaining = CooldownEndTime - Now;

    if (Remaining <= 0.f)
    {
        ClearCooldownUI();
        return;
    }

    // Update Visual (Fill)
    if (ImgCooldownRadial && CooldownTotal > 0.f)
    {
        const float Ratio = FMath::Clamp(Remaining / CooldownTotal, 0.f, 1.f);
        if (CooldownMID)
        {
            CooldownMID->SetScalarParameterValue(TEXT("Fill"), Ratio);
        }
    }

    // Update Text (3s)
    if (TxtCooldown)
    {
        const int32 Seconds = FMath::CeilToInt(Remaining);
        if (Seconds > 0)
        {
            TxtCooldown->SetText(FText::FromString(FString::Printf(TEXT("%ds"), Seconds)));
            TxtCooldown->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            TxtCooldown->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}
