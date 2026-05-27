#include "UI/ComboPopupWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UComboPopupWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 쿨다운 오버레이용 Dynamic Material 생성
    if (Image_Cooldown)
    {
        if (UMaterialInterface* MI = Cast<UMaterialInterface>(Image_Cooldown->GetBrush().GetResourceObject()))
        {
            CooldownMID = UMaterialInstanceDynamic::Create(MI, this);

            FSlateBrush Brush = Image_Cooldown->GetBrush();
            Brush.SetResourceObject(CooldownMID);
            Image_Cooldown->SetBrush(Brush);
        }
        
        // 시작 시 보이지 않게 처리
        Image_Cooldown->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (CooldownMID)
    {
        CooldownMID->SetScalarParameterValue(TEXT("Fill"), 0.f);
    }

    // 시작 시 위젯을 숨김 상태로 조용히 즉각 초기화합니다. (사라짐 애니메이션 번쩍임 방지)
    FinishHideCombo();
}

void UComboPopupWidget::NativeDestruct()
{
    ClearCooldownUI();
    Super::NativeDestruct();
}

void UComboPopupWidget::ShowCombo(TSoftObjectPtr<UTexture2D> NextIcon, float Duration, FText SlotKey, float CooldownRemaining, float CooldownTotalTime)
{
    if (Image_ComboIcon)
    {
        // 소프트 오브젝트 텍스처를 비동기식으로 안전하게 이미지 위젯에 설정합니다.
        Image_ComboIcon->SetBrushFromSoftTexture(NextIcon);
    }

    if (TextBlock_SlotKey)
    {
        // 현재 스킬이 등록된 단축키 번호를 자동으로 텍스트에 표기합니다.
        TextBlock_SlotKey->SetText(SlotKey);
    }

    // [New] 만약 다음 연계 스킬이 쿨다운 상태라면 쿨타임 radial 및 초를 구동합니다.
    if (CooldownRemaining > 0.f)
    {
        if (UWorld* World = GetWorld())
        {
            StartCooldown(CooldownRemaining, World->GetTimeSeconds() + CooldownRemaining);
        }
    }
    else
    {
        ClearCooldownUI();
    }

    // 위젯을 화면에 보이도록 처리합니다.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    // [New] 블루프린트에게 가드 팝업창이 나타났음을 알리는 징검다리 이벤트를 트리거합니다! (애니메이션 등 호출용)
    OnShowComboTriggered();
}

void UComboPopupWidget::HideCombo()
{
    ClearCooldownUI();

    // [New] 블루프린트에게 가드 팝업창이 사라지는 연출을 시작하라고 신호를 보냅니다!
    OnHideComboTriggered();
}

void UComboPopupWidget::FinishHideCombo()
{
    // [New] 블루프린트 사라짐 애니메이션이 다 끝난 안전한 시점에 실제로 위젯을 숨겨 화면과 레이아웃에서 제거합니다.
    SetVisibility(ESlateVisibility::Collapsed);
}

void UComboPopupWidget::StartCooldown(float InDuration, float InEndTime)
{
    if (InDuration <= 0.f)
        return;

    CooldownTotal = InDuration;
    CooldownEndTime = InEndTime;
    bCooldownActive = true;

    // [New] Dynamic Material Instance가 생성되어 있지 않다면 런타임 지연 생성을 시도합니다! (초기 로딩 시점 UMG 타이머 오차 방지)
    if (!CooldownMID && Image_Cooldown)
    {
        if (UMaterialInterface* MI = Cast<UMaterialInterface>(Image_Cooldown->GetBrush().GetResourceObject()))
        {
            CooldownMID = UMaterialInstanceDynamic::Create(MI, this);

            FSlateBrush Brush = Image_Cooldown->GetBrush();
            Brush.SetResourceObject(CooldownMID);
            Image_Cooldown->SetBrush(Brush);
            
            UE_LOG(LogTemp, Log, TEXT("[ComboPopup] 런타임 지연 Dynamic Material Instance가 성공적으로 생성되었습니다."));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[ComboPopup] Image_Cooldown 브러시에서 부모 머티리얼을 찾지 못해 Dynamic Material 생성에 실패했습니다."));
        }
    }

    // 0.05초 주기로 정밀하게 쿨타임 UI 갱신 (Radial이 부드럽게 돌아가도록 함)
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(CooldownTimerHandle, this, &UComboPopupWidget::UpdateCooldownTick, 0.05f, true);
    }

    if (Image_Cooldown)
    {
        Image_Cooldown->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    if (TextBlock_Cooldown)
    {
        TextBlock_Cooldown->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    if (CooldownMID)
    {
        CooldownMID->SetScalarParameterValue(TEXT("Fill"), 1.f);
    }

    // 즉시 한 번 갱신 실행
    UpdateCooldownTick();
}

void UComboPopupWidget::ClearCooldownUI()
{
    bCooldownActive = false;
    CooldownEndTime = 0.f;
    CooldownTotal = 0.f;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CooldownTimerHandle);
    }

    if (Image_Cooldown)
    {
        Image_Cooldown->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (TextBlock_Cooldown)
    {
        TextBlock_Cooldown->SetText(FText::GetEmpty());
        TextBlock_Cooldown->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (CooldownMID)
    {
        CooldownMID->SetScalarParameterValue(TEXT("Fill"), 0.f);
    }
}

void UComboPopupWidget::UpdateCooldownTick()
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
        ClearCooldownUI();
        return;
    }

    // 초 표기 (정수 올림 처리)
    if (TextBlock_Cooldown)
    {
        const int32 Seconds = FMath::CeilToInt(Remaining);
        const FString TextStr = FString::Printf(TEXT("%ds"), Seconds);
        TextBlock_Cooldown->SetText(FText::FromString(TextStr));
    }

    // 머티리얼 파라미터 갱신 (원형 진행도 채우기 파라미터명: Fill)
    if (CooldownMID && CooldownTotal > 0.f)
    {
        const float Ratio = FMath::Clamp(Remaining / CooldownTotal, 0.f, 1.f);
        CooldownMID->SetScalarParameterValue(TEXT("Fill"), Ratio);
    }
}
