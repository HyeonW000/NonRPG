#include "UI/Loading/LoadingScreenWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"

ULoadingScreenWidget::ULoadingScreenWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // 기본 게임 꿀팁 7종 자동 등록
    TipList.Add(TEXT("포션을 퀵슬롯에 등록하면 단축키로 신속하게 체력을 회복할 수 있습니다."));
    TipList.Add(TEXT("파티원과 함께 보스를 공략하면 더 희귀한 전리품을 획득할 수 있습니다."));
    TipList.Add(TEXT("보스가 무적 상태로 돌입하면 거리를 벌리고 패턴을 지켜보세요."));
    TipList.Add(TEXT("공격 중 스페이스바를 누르면 콤보를 캔슬하고 신속하게 회피할 수 있습니다."));
    TipList.Add(TEXT("스킬 포인트는 레벨업할 때마다 주어지며, 스킬창(+)에서 스킬을 강화할 수 있습니다."));
    TipList.Add(TEXT("상대방 플레이어를 우클릭하면 파티 초대 및 1:1 개인 거래를 신청할 수 있습니다."));
    TipList.Add(TEXT("자동 달리기 상태에서도 방향키를 조작하여 자연스럽게 방향을 바꿀 수 있습니다."));
}

void ULoadingScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();

    CurrentProgress = 0.0f;
    bIsFinishing = false;
    FinishElapsed = 0.0f;
    if (ProgressBar_Loading)
    {
        ProgressBar_Loading->SetPercent(0.0f);
    }

    RefreshTipText();
}

void ULoadingScreenWidget::FinishLoading(float InFinishDuration)
{
    if (bIsFinishing) return;

    bIsFinishing = true;
    FinishDuration = FMath::Max(0.1f, InFinishDuration);
    ProgressAtFinishStart = CurrentProgress;
    FinishElapsed = 0.0f;
}

void ULoadingScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bIsFinishing)
    {
        // 1. 스폰 완료 신호 후: 100%까지 빠르게 가속하여 꽉 채움!
        FinishElapsed += InDeltaTime;
        const float Alpha = FMath::Clamp(FinishElapsed / FinishDuration, 0.0f, 1.0f);
        CurrentProgress = FMath::Lerp(ProgressAtFinishStart, 1.0f, Alpha);
    }
    else
    {
        // 2. 맵 로딩 대기 중: 최대 88%까지만 점진적으로 감속하며 부드럽게 대기
        const float TargetMax = 0.88f;
        if (CurrentProgress < TargetMax && FillDuration > 0.0f)
        {
            const float Step = (InDeltaTime / FillDuration) * (1.0f - (CurrentProgress / TargetMax) * 0.5f);
            CurrentProgress = FMath::Min(CurrentProgress + Step, TargetMax);
        }
    }

    if (ProgressBar_Loading)
    {
        ProgressBar_Loading->SetPercent(CurrentProgress);
    }
}

void ULoadingScreenWidget::RefreshTipText()
{
    if (!Text_Tip)
    {
        return;
    }

    if (TipList.Num() > 0)
    {
        const int32 RandomIndex = FMath::RandRange(0, TipList.Num() - 1);
        Text_Tip->SetText(FText::FromString(TipList[RandomIndex]));
    }
    else
    {
        Text_Tip->SetText(FText::FromString(TEXT("던전으로 이동하는 중입니다...")));
    }
}
