#include "UI/UIViewportUtils.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWidget.h"

namespace UIViewportUtils
{
    TSharedPtr<SViewport> GetGameViewportSViewport(UWorld* World)
    {
        if (!World) return nullptr;

        UGameViewportClient* GVC = nullptr;

        // 1) 우선 현재 월드의 로컬 플레이어에서 ViewportClient를 얻기
        if (ULocalPlayer* LP = World->GetFirstLocalPlayerFromController())
        {
            GVC = LP->ViewportClient;
        }

        // 2) 실패하면 전역 GEngine의 GameViewport를 폴백
        if (!GVC && GEngine)
        {
            GVC = GEngine->GameViewport;
        }

        if (!GVC) return nullptr;

        // UE5.5: GetGameViewportWidget()은 TSharedPtr<SWidget> 반환
        TSharedPtr<SWidget> W = GVC->GetGameViewportWidget();
        if (!W.IsValid()) return nullptr;

        // 대개 SViewport이므로 캐스팅 시도
        TSharedPtr<SViewport> SV = StaticCastSharedPtr<SViewport>(W);
        return SV;
    }
}
