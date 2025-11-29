#include "Inventory/ItemDragDropOperation.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "UI/QuickSlotManager.h"
#include "GameFramework/PlayerController.h"

TSharedPtr<SViewport> UItemDragDropOperation::GetGameViewportSViewport(UWorld* World)
{
    if (!World) return nullptr;
    if (UGameViewportClient* GVC = World->GetGameViewport())
    {
        return GVC->GetGameViewportWidget(); // TSharedPtr<SViewport>
    }
    return nullptr;
}

void UItemDragDropOperation::KeepGameFocus(UObject* WorldContext)
{
    if (!WorldContext) return;
    UWorld* World = WorldContext->GetWorld();
    if (!World) return;

    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetWidgetToFocus(nullptr);
        PC->SetInputMode(Mode);
    }

    if (TSharedPtr<SViewport> VP = GetGameViewportSViewport(World))
    {
        const uint32 UserIdx = FSlateApplication::Get().GetUserIndexForKeyboard();
        FSlateApplication::Get().SetUserFocus(
            UserIdx,
            StaticCastSharedRef<SWidget>(VP.ToSharedRef()),
            EFocusCause::SetDirectly);
        FSlateApplication::Get().SetKeyboardFocus(VP, EFocusCause::SetDirectly);
    }
}

void UItemDragDropOperation::Dragged_Implementation(const FPointerEvent& PointerEvent)
{
    KeepGameFocus(this);
    Super::Dragged_Implementation(PointerEvent);
}

void UItemDragDropOperation::Drop_Implementation(const FPointerEvent& PointerEvent)
{
    KeepGameFocus(this);
    Super::Drop_Implementation(PointerEvent);
}

void UItemDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
    KeepGameFocus(this);

    if (bFromQuickSlot && SourceQuickIndex != INDEX_NONE)
    {
        if (UQuickSlotManager* Mgr = SourceQuickManager.Get())
        {
            Mgr->ClearSlot(SourceQuickIndex); // ← 안전하게 해제
        }
    }

    Super::DragCancelled_Implementation(PointerEvent);
}