#include "UI/QuickSlot/QuickSlotSlotWidget.h"
#include "UI/QuickSlot/QuickSlotManager.h"
#include "UI/QuickSlot/QuickSlotBarWidget.h"
#include "UI/Skill/SkillDragDropOperation.h"
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
#include "Skill/SkillTypes.h"
#include "Skill/NonSkillDataAsset.h"
#include "UI/UIViewportUtils.h"

void UQuickSlotSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 포커스 가져가지 않도록 설정 (WASD 이동 끊김 방지)
    SetIsFocusable(false);

    // 쿨다운 오버레이용 MID 생성
    if (CooldownOverlay)
    {
        if (UMaterialInterface* MI = Cast<UMaterialInterface>(CooldownOverlay->Brush.GetResourceObject()))
        {
            CooldownMID = UMaterialInstanceDynamic::Create(MI, this);

            FSlateBrush Brush = CooldownOverlay->GetBrush();
            Brush.SetResourceObject(CooldownMID);
            CooldownOverlay->SetBrush(Brush);
        }

        // 시작은 숨김
        CooldownOverlay->SetVisibility(ESlateVisibility::Collapsed);
    }

    // 초기 Fill 값 0으로
    if (CooldownMID)
    {
        CooldownMID->SetScalarParameterValue(TEXT("Fill"), 0.f);
    }
    
    // 최적화: 쿨타임이 없을 때는 Tick을 꺼둠 (Timer 방식이므로 기본적으로 안 돔)
}

void UQuickSlotSlotWidget::NativeDestruct()
{
    ClearCooldownUI(); 
    Super::NativeDestruct();
}

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
   //  스킬 쿨타임 UI 동기화
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

    // 스킬 동기화
    SetAssignedSkillId(Manager->GetSkillInSlot(QuickIndex));

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

    // === 1) 이 슬롯에 스킬이 배정돼 있으면 → 스킬 드래그 (퀵슬롯 스왑용) ===
    if (!AssignedSkillId.IsNone())
    {
        if (!Manager.IsValid() || QuickIndex < 0)
            return;

        UItemDragDropOperation* Op = NewObject<UItemDragDropOperation>(this);
        Op->bFromQuickSlot = true;
        Op->SourceQuickIndex = QuickIndex;
        Op->SourceQuickManager = Manager.Get();
        Op->Item = nullptr; // 스킬이니까 인벤토리 아이템은 없음

        // 드래그 비주얼용 아이콘: 캐시된 아이콘 or 현재 브러시에서 가져오기
        UTexture2D* Icon = CachedIcon;
        if (!Icon && IconImage)
        {
            if (UObject* ResObj = IconImage->GetBrush().GetResourceObject())
            {
                Icon = Cast<UTexture2D>(ResObj);
            }
        }

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
                }

                Op->DefaultDragVisual = Visual;
                Op->Pivot = EDragPivot::MouseDown;
                Op->Offset = FVector2D::ZeroVector;
            }
        }
        OutOperation = Op;
        return;
    }

    // === 2) 그 외에는 기존 아이템 드래그 로직 그대로 유지 ===
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
                }

                Op->DefaultDragVisual = Visual;
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
        UE_LOG(LogTemp, Warning,
            TEXT("[QuickSlotSlot] OnDrop SKILL: ThisIndex=%d, SkillId=%s"),
            QuickIndex,
            *SkillOp->SkillId.ToString());

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
    if (!Op || !Manager.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[QuickSlotSlot] OnDrop ITEM: ThisIndex=%d, ManagerValid=%d, OpValid=%d"),
            QuickIndex,
            Manager.IsValid() ? 1 : 0,
            Op ? 1 : 0);
        return false;
    }
    if (Op->bFromQuickSlot && Op->SourceQuickIndex != INDEX_NONE)
    {
        const int32 SrcIndex = Op->SourceQuickIndex;
        const int32 DstIndex = QuickIndex;

        UE_LOG(LogTemp, Warning,
            TEXT("[QuickSlotSlot] OnDrop FROM QUICK: Src=%d -> Dst=%d"),
            SrcIndex, DstIndex);

        // 같은 슬롯이면 아무 것도 안 하고 성공 처리
        if (SrcIndex == DstIndex)
            return true;

        // 스킬 할당도 같이 스왑
        if (UQuickSlotBarWidget* Bar = GetTypedOuter<UQuickSlotBarWidget>())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[QuickSlotSlot]  -> SwapSkillAssignment(%d, %d)"), SrcIndex, DstIndex);
            Bar->SwapSkillAssignment(SrcIndex, DstIndex);
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[QuickSlotSlot]  -> Bar NOT FOUND (GetTypedOuter<UQuickSlotBarWidget> failed)"));
        }

        // 아이템/인벤토리 쪽 슬롯 스왑
        const bool bSwapped = Manager->SwapSlots(SrcIndex, DstIndex);

        UE_LOG(LogTemp, Warning,
            TEXT("[QuickSlotSlot]  -> Manager->SwapSlots(%d, %d) = %s"),
            SrcIndex, DstIndex,
            bSwapped ? TEXT("TRUE") : TEXT("FALSE"));

        return bSwapped;
    }

    if (Op->SourceInventory && Op->SourceIndex != INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[QuickSlotSlot] OnDrop FROM INVENTORY: Dst=%d, SrcIdx=%d"),
            QuickIndex, Op->SourceIndex);

        return Manager->AssignFromInventory(QuickIndex, Op->SourceInventory, Op->SourceIndex);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[QuickSlotSlot] OnDrop FALLTHROUGH: ThisIndex=%d"), QuickIndex);

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
    
    // 타이머 시작 (0.05초 단위, 20fps 정도면 충분)
    // 부드러운 UI를 원하면 0.01f 등으로 설정
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(CooldownTimerHandle, this, &UQuickSlotSlotWidget::UpdateCooldownTick, 0.05f, true);
    }

    if (CooldownOverlay)
    {
        CooldownOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    if (CooldownText)
    {
        CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    if (CooldownMID)
    {
        CooldownMID->SetScalarParameterValue(TEXT("Fill"), 1.f);
    }
    
    // 시작하자마자 1회 갱신
    UpdateCooldownTick();
}

void UQuickSlotSlotWidget::ClearCooldownUI()
{
    bCooldownActive = false;
    CooldownEndTime = 0.f;
    CooldownTotal = 0.f;

    // 타이머 해제
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CooldownTimerHandle);
    }

    if (CooldownOverlay)
    {
        CooldownOverlay->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (CooldownText)
    {
        CooldownText->SetText(FText::GetEmpty());
        CooldownText->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (CooldownMID)
    {
        CooldownMID->SetScalarParameterValue(TEXT("Fill"), 0.f);
    }
}

void UQuickSlotSlotWidget::UpdateCooldownTick()
{
    if (!bCooldownActive)
    {
        ClearCooldownUI();
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
        return;

    const float Now = World->GetTimeSeconds();
    const float Remaining = CooldownEndTime - Now;

    if (Remaining <= 0.f)
    {
        // 쿨타임 끝
        ClearCooldownUI();
        return;
    }

    // 숫자 갱신 (ceil 로 1,2,3초 단위 느낌)
    if (CooldownText)
    {
        const int32 Seconds = FMath::CeilToInt(Remaining);
        const FString TextStr = FString::Printf(TEXT("%ds"), Seconds);
        CooldownText->SetText(FText::FromString(TextStr));
    }

    // 머티리얼 Fill 갱신 (남은 비율 1.0 → 0.0)
    if (CooldownMID && CooldownTotal > 0.f)
    {
        const float Ratio = FMath::Clamp(Remaining / CooldownTotal, 0.f, 1.f);
        CooldownMID->SetScalarParameterValue(TEXT("Fill"), Ratio);
    }
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

void UQuickSlotSlotWidget::SetAssignedSkillId(FName NewId)
{
    AssignedSkillId = NewId;

    // 스킬이 사라지는 경우(빈칸이 되는 경우) → 쿨타임 + 아이콘 정리
    if (AssignedSkillId.IsNone())
    {
        // 쿨타임 UI 끄기
        ClearCooldownUI();

        // 아이콘도 비워줌 (아이템 있으면 나중에 OnQuickSlotChanged → UpdateVisual 에서 다시 세팅됨)
        if (IconImage)
        {
            FSlateBrush Brush = IconImage->GetBrush();
            Brush.SetResourceObject(nullptr);
            Brush.ImageSize = FVector2D::ZeroVector;
            IconImage->SetBrush(Brush);
            IconImage->SetVisibility(ESlateVisibility::Collapsed);
        }

        return;
    }

    // 스킬이 셋팅되는 경우 → 쿨타임/아이콘 동기화
    ResyncCooldownFromSkill();   // 또는 기존에 쓰던 함수 이름
    UpdateSkillIconFromData();   // 스킬 DA에서 Icon 다시 가져와서 세팅하는 함수
}

void UQuickSlotSlotWidget::ResyncCooldownFromSkill()
{
    // 스킬 없으면 쿨타임 UI 꺼버림
    if (AssignedSkillId.IsNone())
    {
        ClearCooldownUI();
        return;
    }

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
}

void UQuickSlotSlotWidget::UpdateSkillIconFromData()
{
    if (!IconImage)
        return;

    // 이 슬롯에 스킬이 없는 경우 → 아이콘은 건드리지 않음 (아이템일 수도 있으니까)
    if (AssignedSkillId.IsNone())
        return;

    UTexture2D* NewTex = nullptr;

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            if (USkillManagerComponent* SkillMgr =
                Pawn->FindComponentByClass<USkillManagerComponent>())
            {
                if (const USkillDataAsset* DA = SkillMgr->GetDataAsset())
                {
                    if (const FSkillRow* Row = DA->Skills.Find(AssignedSkillId))
                    {
                        if (!Row->Icon.IsNull())
                        {
                            NewTex = Row->Icon.Get();
                            if (!NewTex)
                            {
                                NewTex = Row->Icon.LoadSynchronous();
                            }
                        }
                    }
                }
            }
        }
    }

    if (!NewTex)
    {
        // 아이콘 못 찾으면 그대로 두거나, 지우고 싶으면 여기서 처리
        return;
    }

    // BP에서 설정한 Rounded Box 브러시 유지 + 텍스처만 교체
    FSlateBrush Brush = IconImage->GetBrush();
    Brush.SetResourceObject(NewTex);

    IconImage->SetBrush(Brush);
    IconImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    CachedIcon = NewTex;
}