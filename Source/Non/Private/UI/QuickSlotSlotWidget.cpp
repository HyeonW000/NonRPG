#include "UI/QuickSlotSlotWidget.h"
#include "UI/QuickSlotManager.h"
#include "UI/QuickSlotBarWidget.h"
#include "UI/SkillDragDropOperation.h"
#include "Inventory/ItemDragDropOperation.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryItem.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Widgets/SViewport.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Skill/SkillManagerComponent.h"
#include "UI/UIViewportUtils.h"

void UQuickSlotSlotWidget::SetManager(UQuickSlotManager* InManager)
{
    Manager = InManager;
}

void UQuickSlotSlotWidget::UpdateVisual(UInventoryItem* Item)
{
    // 스킬이 배정된 칸이면, 인벤토리/매니저 갱신은 무시한다.
    if (!AssignedSkillId.IsNone())
    {
        UE_LOG(LogTemp, Log,
            TEXT("[QuickSlotSlot] UpdateVisual SKIP (SkillAssigned). QuickIndex=%d, SkillId=%s"),
            QuickIndex,
            *AssignedSkillId.ToString());

        // 스킬은 개수 텍스트도 안 쓸 거면 여기서 같이 숨겨도 됨
        if (CountText)
        {
            CountText->SetText(FText::GetEmpty());
            CountText->SetVisibility(ESlateVisibility::Collapsed);
        }

        return;
    }

    int32 Total = 0;
    bool bAssigned = true;
    if (Manager.IsValid() && QuickIndex >= 0)
    {
        Total = Manager->GetTotalCountForSlot(QuickIndex);
        bAssigned = Manager->IsSlotAssigned(QuickIndex);
    }
    else if (Item)
    {
        Total = Item->GetStackCount();
    }

    if (IconImage)
    {
        if (Item && Item->GetIcon())
        {
            UTexture2D* NewTex = Item->GetIcon();

            // BP에서 설정한 Rounded Box 브러시 유지 + 텍스처만 교체
            FSlateBrush Brush = IconImage->GetBrush();
            Brush.SetResourceObject(NewTex);

            const float Size = 64.f; // 슬롯 크기에 맞게 조정
            Brush.ImageSize = FVector2D(Size, Size);

            IconImage->SetBrush(Brush);
            CachedIcon = NewTex;
            IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            if (bAssigned && (CachedIcon != nullptr || IconImage->GetBrush().GetResourceObject() != nullptr))
            {
                IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
            }
            else
            {
                CachedIcon = nullptr;

                // 기존 브러시 유지 + 리소스만 비우기
                FSlateBrush Brush = IconImage->GetBrush();
                Brush.SetResourceObject(nullptr);
                Brush.ImageSize = FVector2D::ZeroVector;

                IconImage->SetBrush(Brush);
                IconImage->SetVisibility(ESlateVisibility::Collapsed);
            }
        }

        if (IconImage->GetVisibility() != ESlateVisibility::Collapsed)
        {
            const float Opacity = (Total > 0) ? 1.0f : 0.35f;
            IconImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, Opacity));
        }
    }

    if (CountText)
    {
        if (Total >= 1)
        {
            CountText->SetText(FText::AsNumber(Total));
            CountText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            CountText->SetText(FText::GetEmpty());
            CountText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // -------------------------
   //  스킬 쿨타임 UI 동기화
   // -------------------------

   // 1) 슬롯에 스킬이 없는 경우 → 쿨타임 UI 전부 끔
    if (AssignedSkillId.IsNone())
    {
        ClearCooldownUI();
    }
    else
    {
        // 2) 슬롯에 스킬이 있다 → SkillManager 에서 남은 쿨타임 조회
        float Remaining = 0.f;
        bool bHasCooldown = false;

        if (APlayerController* PC = GetOwningPlayer())
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                if (USkillManagerComponent* SkillMgr = Pawn->FindComponentByClass<USkillManagerComponent>())
                {
                    // IsOnCooldown(SkillId, OutRemaining) 같은 함수가 있다고 가정
                    bHasCooldown = SkillMgr->IsOnCooldown(AssignedSkillId, Remaining);
                }
            }
        }

        if (bHasCooldown && Remaining > 0.f)
        {
            // 현재 시간 기준으로 다시 StartCooldown
            float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
            StartCooldown(Remaining, Now + Remaining);
        }
        else
        {
            // 쿨타임 없음 → UI 끔
            ClearCooldownUI();
        }
    }
    UpdateVisualBP(Item);
}

void UQuickSlotSlotWidget::Refresh()
{
    if (!Manager.IsValid() || QuickIndex < 0)
    {
        UpdateVisual(nullptr);
        return;
    }

    UInventoryItem* Item = Manager->ResolveItem(QuickIndex);

    if (Item && Item->GetStackCount() <= 0)
    {
        Item = nullptr;
    }

    UpdateVisual(Item);
}

FReply UQuickSlotSlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    if (E.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (APlayerController* PC = GetOwningPlayer())
        {
            FInputModeGameAndUI Mode;
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Mode.SetHideCursorDuringCapture(false);
            Mode.SetWidgetToFocus(nullptr);
            PC->SetInputMode(Mode);
        }

        FEventReply ER = UWidgetBlueprintLibrary::DetectDragIfPressed(E, this, EKeys::LeftMouseButton);
        FReply Reply = ER.NativeReply;

        if (TSharedPtr<SViewport> VP = UIViewportUtils::GetGameViewportSViewport(GetWorld()))
        {
            Reply = Reply.SetUserFocus(StaticCastSharedRef<SWidget>(VP.ToSharedRef()), EFocusCause::SetDirectly);
            FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
        }
        return Reply;
    }
    return Super::NativeOnPreviewMouseButtonDown(G, E);
}

void UQuickSlotSlotWidget::NativeOnDragDetected(const FGeometry& G, const FPointerEvent& E, UDragDropOperation*& OutOperation)
{
    OutOperation = nullptr;
    if (!Manager.IsValid() || QuickIndex < 0) return;

    if (UInventoryItem* Item = Manager->ResolveItem(QuickIndex))
    {
        UItemDragDropOperation* Op = NewObject<UItemDragDropOperation>(this);
        Op->bFromQuickSlot = true;
        Op->SourceQuickIndex = QuickIndex;
        Op->SourceQuickManager = Manager.Get();
        Op->Item = Item;

        UTexture2D* Icon = Item->GetIcon();

        if (DragVisualClass)
        {
            if (UUserWidget* Visual = CreateWidget<UUserWidget>(GetWorld(), DragVisualClass))
            {
                Visual->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

                if (UImage* Img = Cast<UImage>(Visual->GetWidgetFromName(TEXT("IconImage"))))
                {
                    if (Icon)
                    {
                        Img->SetBrushFromTexture(Icon);

                        const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
                        const FVector2D TexSize(Icon->GetSizeX(), Icon->GetSizeY());
                        if (Scale > 0.f && TexSize.X > 0.f && TexSize.Y > 0.f)
                        {
                            Visual->SetDesiredSizeInViewport(TexSize / Scale);
                        }
                    }
                    else
                    {
                        Img->SetBrush(FSlateBrush());
                    }
                }

                Op->DefaultDragVisual = Visual;
                Op->Pivot = EDragPivot::MouseDown;
                Op->Offset = FVector2D::ZeroVector;
            }
        }
        else
        {
            if (Icon)
            {
                UImage* Img = NewObject<UImage>(this);
                Img->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
                FSlateBrush Brush;
                Brush.SetResourceObject(Icon);
                Brush.ImageSize = FVector2D(Icon->GetSizeX(), Icon->GetSizeY());
                Img->SetBrush(Brush);

                Op->DefaultDragVisual = Img;
                Op->Pivot = EDragPivot::MouseDown;
                Op->Offset = FVector2D::ZeroVector;
            }
        }

        OutOperation = Op;
    }
}

bool UQuickSlotSlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    // 인벤토리 아이템 or 스킬 둘 다 허용
    if (Cast<UItemDragDropOperation>(InOperation) != nullptr)
    {
        return true;
    }
    if (Cast<USkillDragDropOperation>(InOperation) != nullptr)
    {
        return true;
    }
    return false;
}


bool UQuickSlotSlotWidget::NativeOnDrop(const FGeometry& G, const FDragDropEvent& E, UDragDropOperation* InOp)
{
    // 1) 스킬 드롭인지 먼저 체크
    if (USkillDragDropOperation* SkillOp = Cast<USkillDragDropOperation>(InOp))
    {
        if (SkillOp->SkillId.IsNone())
            return false;

        AssignedSkillId = SkillOp->SkillId;

        // 아이콘 로드
        UTexture2D* IconTex = nullptr;
        if (!SkillOp->Icon.IsNull())
        {
            IconTex = SkillOp->Icon.Get();
            if (!IconTex)
            {
                IconTex = SkillOp->Icon.LoadSynchronous();
            }
        }

        if (IconImage && IconTex)
        {
            // BP에서 설정해둔 Rounded Box 설정은 유지하고,
            // 텍스처만 바꾼다.
            FSlateBrush Brush = IconImage->GetBrush();
            Brush.SetResourceObject(IconTex);

            IconImage->SetBrush(Brush);
            IconImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            CachedIcon = IconTex;

            UE_LOG(LogTemp, Log,
                TEXT("[QuickSlotSlot] Skill %s set brush (Rounded). DrawAs=%d"),
                *AssignedSkillId.ToString(),
                (int32)Brush.DrawAs);
        }

        // 다른 슬롯에 같은 스킬 있으면 제거
        if (UQuickSlotBarWidget* Bar = GetTypedOuter<UQuickSlotBarWidget>())
        {
            Bar->ClearSkillFromOtherSlots(AssignedSkillId, this);
        }

        // 매니저에 등록
        if (Manager.IsValid())
        {
            Manager->AssignSkillToSlot(QuickIndex, AssignedSkillId);
        }

        // 스킬 쿨타임 동기화 추가 부분 
        float Remaining = 0.f;
        bool bOnCooldown = false;

        if (APlayerController* PC = GetOwningPlayer())
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                if (USkillManagerComponent* SkillMgr =
                    Pawn->FindComponentByClass<USkillManagerComponent>())
                {
                    bOnCooldown = SkillMgr->IsOnCooldown(AssignedSkillId, Remaining);
                }
            }
        }

        if (bOnCooldown && Remaining > 0.f)
        {
            const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
            StartCooldown(Remaining, Now + Remaining);
        }
        else
        {
            ClearCooldownUI();
        }

        return true;
    }

    // 2) 아니면 기존 아이템 드롭 처리
    UItemDragDropOperation* Op = Cast<UItemDragDropOperation>(InOp);
    if (!Op || !Manager.IsValid()) return false;

    if (Op->bFromQuickSlot && Op->SourceQuickIndex != INDEX_NONE)
    {
        if (Manager.IsValid())
        {
            const int32 SrcIndex = Op->SourceQuickIndex;
            const int32 DstIndex = QuickIndex;

            if (SrcIndex != DstIndex)
            {
                const bool bSwapped = Manager->SwapSlots(SrcIndex, DstIndex);
                if (bSwapped)
                {
                    // SwapSlots 안에서 OnQuickSlotChanged 로 두 슬롯 UI까지 갱신됨
                    return true;
                }
            }
        }
        // 스왑 실패하면 아이템 재배치도 안 함
        return false;
    }

    if (Op->SourceInventory && Op->SourceIndex != INDEX_NONE)
    {
        return Manager->AssignFromInventory(QuickIndex, Op->SourceInventory, Op->SourceIndex);
    }

    return false;
}

void UQuickSlotSlotWidget::StartCooldown(float InDuration, float InEndTime)
{
    if (InDuration <= 0.f)
        return;

    CooldownTotal = InDuration;

    if (InEndTime > 0.f)
    {
        CooldownEndTime = InEndTime;
    }
    else if (UWorld* World = GetWorld())
    {
        CooldownEndTime = World->GetTimeSeconds() + InDuration;
    }
    else
    {
        return;
    }

    bCooldownActive = true;

    if (CooldownOverlay)
    {
        CooldownOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    if (CooldownText)
    {
        CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

void UQuickSlotSlotWidget::ClearCooldownUI()
{
    bCooldownActive = false;
    CooldownEndTime = 0.f;
    CooldownTotal = 0.f;

    if (CooldownOverlay)
    {
        CooldownOverlay->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (CooldownText)
    {
        CooldownText->SetText(FText::GetEmpty());
        CooldownText->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UQuickSlotSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bCooldownActive)
        return;

    UWorld* World = GetWorld();
    if (!World)
        return;

    const float Now = World->GetTimeSeconds();
    const float Remaining = CooldownEndTime - Now;

    if (Remaining <= 0.f)
    {
        bCooldownActive = false;

        if (CooldownOverlay)
        {
            CooldownOverlay->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (CooldownText)
        {
            CooldownText->SetText(FText::GetEmpty());
            CooldownText->SetVisibility(ESlateVisibility::Collapsed);
        }
        return;
    }

    // 숫자 갱신 (ceil 로 1,2,3초 단위 느낌)
    if (CooldownText)
    {
        const int32 Seconds = FMath::CeilToInt(Remaining);
        CooldownText->SetText(FText::AsNumber(Seconds));
    }

    // 필요하면 오버레이 투명도/효과도 여기서 조절 가능
    // ex) 남은 비율 = Remaining / CooldownTotal;
}


void UQuickSlotSlotWidget::ClearSkillAssignment()
{
    AssignedSkillId = NAME_None;
    CachedIcon = nullptr;

    if (IconImage)
    {
        // 기존 브러시 유지 + 리소스만 비우기
        FSlateBrush Brush = IconImage->GetBrush();
        Brush.SetResourceObject(nullptr);
        Brush.ImageSize = FVector2D::ZeroVector;
        IconImage->SetBrush(Brush);

        IconImage->SetVisibility(ESlateVisibility::Collapsed);
    }

    // 매니저에도 반영
    if (Manager.IsValid())
    {
        Manager->ClearSkillFromSlot(QuickIndex);
    }

    UE_LOG(LogTemp, Log,
        TEXT("[QuickSlotSlot] ClearSkillAssignment: Slot=%d"),
        QuickIndex);

    ClearCooldownUI();
}