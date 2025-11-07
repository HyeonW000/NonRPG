#pragma once
#include "CoreMinimal.h"
#include "Widgets/SViewport.h"

namespace UIViewportUtils
{
    /** 현재 월드의 게임 SViewport 위젯을 얻는다. 실패 시 nullptr */
    TSharedPtr<SViewport> GetGameViewportSViewport(UWorld* World);
}
