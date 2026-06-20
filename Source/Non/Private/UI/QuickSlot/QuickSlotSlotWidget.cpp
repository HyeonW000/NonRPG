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
        if (UMaterialInterface* MI = Cast<UMaterialInterface>(CooldownOverlay->GetBrush().GetResourceObject()))
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
    
    // [New] 생성 시점에 소유자 캐릭터의 콤보 델리게이트와 연동을 시도합니다.
    BindSkillComboDelegate();
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
            if (bIsDraggingThisSlot)
            {
                // 드래그 중인 슬롯: 살짝 어두운 반투명 회색조 처리
                IconImage->SetColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.2f, 0.4f));
            }
            else
            {
                const float Opacity = (Total > 0 || !AssignedSkillId.IsNone()) ? 1.0f : 0.35f;
                IconImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, Opacity));
            }
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
    // 3) 아이템 쿨타임 UI 동기화 (스킬이 아닐 때)
    if (AssignedSkillId.IsNone() && Item)
    {
        // 델리게이트 바인딩 시도 (아직 안 했으면)
        BindInventoryDelegate();

        // 현재 상태 확인
        if (BoundInventoryComp.IsValid())
        {
            const FName GroupId = Item->CachedRow.Consumable.CooldownGroupId;
            float Val = 0.f, TotalDuration = 0.f;
            if (BoundInventoryComp->GetCooldownRemaining(GroupId, Val, TotalDuration))
            {
                // 현재 시간 기준 EndTime 역산
                float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
                StartCooldown(TotalDuration, Now + Val);
            }
            else
            {
                ClearCooldownUI();
            }
        }
    }
    
    UpdateVisualBP(Item);
}

void UQuickSlotSlotWidget::BindInventoryDelegate()
{
    if (BoundInventoryComp.IsValid()) return;

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            if (UInventoryComponent* Inv = Pawn->FindComponentByClass<UInventoryComponent>())
            {
                BoundInventoryComp = Inv;
                Inv->OnCooldownStarted.AddDynamic(this, &UQuickSlotSlotWidget::OnInventoryCooldownStarted);
            }
        }
    }
}

void UQuickSlotSlotWidget::OnInventoryCooldownStarted(FName GroupId, float Duration, float EndTime)
{
    // 현재 이 슬롯에 할당된 아이템이 해당 GroupId인지 확인
    if (!AssignedSkillId.IsNone()) return; // 스킬 슬롯이면 무시

    if (!Manager.IsValid() || QuickIndex < 0) return;
    
    UInventoryItem* Item = Manager->ResolveItem(QuickIndex);
    if (Item && Item->CachedRow.Consumable.CooldownGroupId == GroupId)
    {
        StartCooldown(Duration, EndTime);
    }
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

    // [New] 퀵슬롯 리프레시 시점에 콤보 델리게이트를 새로 고쳐 바인딩합니다.
    BindSkillComboDelegate();
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

        // 드래그 해제 또는 드롭 완료 시 원래 아이콘 복구용 바인딩
        Op->OnDragCancelled.AddDynamic(this, &UQuickSlotSlotWidget::OnDragOperationEnded);
        Op->OnDrop.AddDynamic(this, &UQuickSlotSlotWidget::OnDragOperationEnded);

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

                Visual->SetRenderOpacity(0.9f);
                Op->DefaultDragVisual = Visual;
                Op->Pivot = EDragPivot::MouseDown;
                Op->Offset = FVector2D::ZeroVector;
            }
        }

        bIsDraggingThisSlot = true;
        Refresh(); // 드래그 시작 시 슬롯 비주얼 갱신 (회색조)

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

        // 드래그 해제 또는 드롭 완료 시 원래 아이콘 복구용 바인딩
        Op->OnDragCancelled.AddDynamic(this, &UQuickSlotSlotWidget::OnDragOperationEnded);
        Op->OnDrop.AddDynamic(this, &UQuickSlotSlotWidget::OnDragOperationEnded);

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

                Visual->SetRenderOpacity(0.9f);
                Op->DefaultDragVisual = Visual;
                Op->Pivot = EDragPivot::MouseDown;
                Op->Offset = FVector2D::ZeroVector;
            }
        }

        bIsDraggingThisSlot = true;
        Refresh(); // 드래그 시작 시 슬롯 비주얼 갱신 (회색조)

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

        return false;
    }
    if (Op->bFromQuickSlot && Op->SourceQuickIndex != INDEX_NONE)
    {
        const int32 SrcIndex = Op->SourceQuickIndex;
        const int32 DstIndex = QuickIndex;



        // 같은 슬롯이면 아무 것도 안 하고 성공 처리
        if (SrcIndex == DstIndex)
            return true;

        // 스킬 할당도 같이 스왑
        if (UQuickSlotBarWidget* Bar = GetTypedOuter<UQuickSlotBarWidget>())
        {

            Bar->SwapSkillAssignment(SrcIndex, DstIndex);
        }
        else
        {

        }

        // 아이템/인벤토리 쪽 슬롯 스왑
        const bool bSwapped = Manager->SwapSlots(SrcIndex, DstIndex);



        return bSwapped;
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

    // [New] 스킬 재지정(할당) 시점에 콤보 델리게이트 감청을 새로 고침합니다.
    BindSkillComboDelegate();
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
                    // [New] 콤보 연계 대기 상태이면 자동으로 다음 연계 스킬 아이콘으로 스위칭하여 표시합니다!
                    FName SkillToShow = SkillMgr->GetActiveComboSkillId(AssignedSkillId);
                    
                    // 만약 연계 스킬로 스위칭하려는데, 그 스킬의 레벨이 0 이하라면(학습하지 않았다면) 원래 스킬을 보여줍니다!
                    if (SkillToShow != AssignedSkillId && SkillMgr->GetSkillLevel(SkillToShow) <= 0)
                    {
                        SkillToShow = AssignedSkillId;
                    }
                    
                    if (const FSkillRow* Row = DA->Skills.Find(SkillToShow))
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

void UQuickSlotSlotWidget::BindSkillComboDelegate()
{
    // 스킬이 지정되어 있지 않은 슬롯은 바인딩을 건너뜁니다.
    if (AssignedSkillId.IsNone()) return;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    APawn* Pawn = PC->GetPawn();
    if (!Pawn)
    {
        // 폰이 아직 생성되지 않았다면, 0.1초 뒤에 다시 시도하도록 타이머를 겁니다! (무조건 바인딩될 때까지 반복 시도)
        if (UWorld* World = GetWorld())
        {
            FTimerHandle LazyBindHandle;
            World->GetTimerManager().SetTimer(LazyBindHandle, this, &UQuickSlotSlotWidget::BindSkillComboDelegate, 0.1f, false);
        }
        return;
    }

    USkillManagerComponent* SkillMgr = Pawn->FindComponentByClass<USkillManagerComponent>();
    if (!SkillMgr)
    {
        // 폰에 없으면 플레이어 컨트롤러에서 한 번 더 찾아봅니다.
        SkillMgr = PC->FindComponentByClass<USkillManagerComponent>();
        if (!SkillMgr)
        {
            if (AController* PawnCtrl = Pawn->GetController())
            {
                SkillMgr = PawnCtrl->FindComponentByClass<USkillManagerComponent>();
            }
        }
    }

    if (!SkillMgr)
    {
        // 스킬매니저가 양쪽 어디에도 아직 부착되지 않았다면, 0.1초 뒤에 다시 시도합니다.
        if (UWorld* World = GetWorld())
        {
            FTimerHandle LazyBindHandle;
            World->GetTimerManager().SetTimer(LazyBindHandle, this, &UQuickSlotSlotWidget::BindSkillComboDelegate, 0.1f, false);
        }
        return;
    }

    // 중복 등록 방지 후 안전하게 다이내믹 델리게이트를 연동합니다.
    SkillMgr->OnComboWindowChanged.RemoveDynamic(this, &UQuickSlotSlotWidget::OnComboWindowChangedHandler);
    SkillMgr->OnComboWindowChanged.AddDynamic(this, &UQuickSlotSlotWidget::OnComboWindowChangedHandler);
    

}

void UQuickSlotSlotWidget::OnComboWindowChangedHandler(FName BaseSkillId, FName NextSkillId, float Duration, float CooldownRemaining, float InCooldownTotal)
{
    // 현재 이 퀵슬롯 슬롯에 배정되어 있는 스킬 ID가 델리게이트가 쏘아준 선행 스킬 ID(BaseSkillId)와 같을 때에만 반응합니다.
    if (!AssignedSkillId.IsNone() && AssignedSkillId == BaseSkillId)
    {
        // 콤보 창이 열렸거나(NextSkillId 유효) 만료되었을 때(NextSkillId == NAME_None), 실시간으로 아이콘을 갱신합니다.
        UpdateSkillIconFromData();
        

    }
}

void UQuickSlotSlotWidget::OnDragOperationEnded(UDragDropOperation* Operation)
{
    bIsDraggingThisSlot = false;
    Refresh();
}