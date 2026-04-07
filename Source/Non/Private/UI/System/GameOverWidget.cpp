#include "UI/System/GameOverWidget.h"
#include "Components/Button.h"
#include "Core/NonPlayerController.h"

void UNonGameOverWidget::NativeConstruct() {
    Super::NativeConstruct();

    if (Btn_RespawnInPlace) {
        Btn_RespawnInPlace->OnClicked.AddDynamic(this, &ThisClass::OnRespawnInPlaceClicked);
    }

    if (Btn_RespawnAtNearest) {
        Btn_RespawnAtNearest->OnClicked.AddDynamic(this, &ThisClass::OnRespawnAtNearestClicked);
    }
}

void UNonGameOverWidget::OnRespawnInPlaceClicked() {
    if (ANonPlayerController* PC = Cast<ANonPlayerController>(GetOwningPlayer())) {
        // 제자리 부활 요청 (true)
        PC->ServerRespawnPlayer(true);
    }
}

void UNonGameOverWidget::OnRespawnAtNearestClicked() {
    if (ANonPlayerController* PC = Cast<ANonPlayerController>(GetOwningPlayer())) {
        // 근처 부활 지점(PlayerStart) 부활 요청 (false)
        PC->ServerRespawnPlayer(false);
    }
}
