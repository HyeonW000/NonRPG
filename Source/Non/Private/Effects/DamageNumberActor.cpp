// DamageNumberActor.cpp

#include "Effects/DamageNumberActor.h"
#include "Components/WidgetComponent.h"
#include "UI/DamageNumberWidget.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/UserWidget.h"

static inline UDamageNumberWidget* GetDNWidget(UWidgetComponent* Comp)
{
    return Comp ? Cast<UDamageNumberWidget>(Comp->GetUserWidgetObject()) : nullptr;
}

ADamageNumberActor::ADamageNumberActor()
{
    PrimaryActorTick.bCanEverTick = true;
    SetCanBeDamaged(false);
    bReplicates = false;

    WidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComp"));
    SetRootComponent(WidgetComp);

    WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
    WidgetComp->SetDrawAtDesiredSize(true);
    WidgetComp->SetDrawSize(FIntPoint(120, 40));
    WidgetComp->SetPivot(FVector2D(0.5f, 0.5f));
    WidgetComp->SetWidgetClass(UDamageNumberWidget::StaticClass());
    WidgetComp->SetGenerateOverlapEvents(false);
    WidgetComp->SetIsReplicated(false);
    WidgetComp->bReceivesDecals = false;
}

void ADamageNumberActor::InitWithFlags(float InAmount, bool bInCritical)
{
    bIsDodge = false;

    const FLinearColor UseColor = bInCritical ? CriticalDamageColor : NormalDamageColor;
    const int32       UseFont = bInCritical ? FontSize_Crit : NumberFontSize;

    PendingDamage = InAmount;
    PendingColor = UseColor;
    PendingFontSize = UseFont;

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        W->SetupNumber(PendingDamage, PendingColor, PendingFontSize);
        W->SetOutline(OutlineSize, OutlineColor);
    }
}

void ADamageNumberActor::Init(float InDamage, const FLinearColor& InColor, int32 InFontSize)
{
    PendingDamage = InDamage;
    PendingColor = InColor;
    PendingFontSize = InFontSize;

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        W->SetupNumber(PendingDamage, PendingColor, PendingFontSize);
        W->SetOutline(OutlineSize, OutlineColor);
    }
}

void ADamageNumberActor::InitAsLabel(const FText& InText, const FLinearColor& InColor, int32 InFontSize)
{
    bPendingLabel = true;
    PendingLabel = InText;
    PendingColor = InColor;
    PendingFontSize = InFontSize;

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        W->SetupLabel(PendingLabel, PendingColor, PendingFontSize);
        W->SetOutline(OutlineSize, OutlineColor);
        bPendingLabel = false;
    }
}

void ADamageNumberActor::SetupNumeric(float InValue, bool bCritical, const FLinearColor& /*InColor*/)
{
    bIsDodge = false;

    // 무조건 BP에서 세팅한 NormalDamageColor / CriticalDamageColor 사용
    const FLinearColor UseColor = bCritical ? CriticalDamageColor : NormalDamageColor;
    const int32 UseFont = bCritical ? FontSize_Crit : NumberFontSize;

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        W->SetupNumber(InValue, UseColor, UseFont);
        W->SetOutline(OutlineSize, OutlineColor);
    }
}

void ADamageNumberActor::SetupAsDodge()
{
    bIsDodge = true;

    const FLinearColor UseColor = DodgeTextColor;
    const int32        UseFont = DodgeFontSize;

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        W->SetupLabel(FText::FromString(TEXT("Dodge")), UseColor, UseFont);
        W->SetOutline(OutlineSize, OutlineColor);
    }
}

void ADamageNumberActor::BeginPlay()
{
    Super::BeginPlay();

    if (!WidgetComp->GetUserWidgetObject())
    {
        WidgetComp->InitWidget();
    }

    BaseLoc = GetActorLocation();

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        if (PendingDamage != 0.f)
        {
            W->SetupNumber(PendingDamage, PendingColor, PendingFontSize);
        }

        if (bPendingLabel)
        {
            W->SetupLabel(PendingLabel, PendingColor, PendingFontSize);
            bPendingLabel = false;
        }

        W->SetOutline(OutlineSize, OutlineColor);
    }

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            WidgetComp->SetOwnerPlayer(LP);
        }
        
        // [Final Fix] 스크린 스페이스 및 엔진 컬링 방지 설정 강제
        WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
        WidgetComp->SetDrawAtDesiredSize(true);
        WidgetComp->SetTickWhenOffscreen(true);
        
        // 보스 몸체 안에 중심점이 있어도 가려진 것으로 간주하지 않도록 가상 범위를 10배 키웁니다.
        WidgetComp->SetBoundsScale(10.0f);

        const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
        const FRotator Look = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), Eye);
        SetActorRotation(Look);

        // [Fix] 생성 시점의 좌표를 저장 (상승 애니메이션의 기준점)
        BaseLoc = GetActorLocation();

        // [Update] 거대 보스전 시안성 확보를 위해 1.5배 크기로 상향
        SetActorScale3D(FVector(1.5f));
    }
}

void ADamageNumberActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    Age += DeltaSeconds;
    const float T = FMath::Clamp(Age / FMath::Max(0.01f, LifeTime), 0.f, 1.f);
    UpdateVisual(T);

    // 카메라 바라보게
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
        const FRotator Look = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), Eye);
        SetActorRotation(Look);
    }

    if (Age >= LifeTime)
    {
        Destroy();
    }
}

void ADamageNumberActor::UpdateVisual(float T)
{
    // 위로 상승용 Z 이동값
    const float UpValue = RiseSpeed * Age;
    
    // [Fix] 카메라의 오른쪽 방향(Right)을 계산하여 좌우 흔들림이 항상 내 화면 기준으로 평면적으로 보이게 함
    FVector SidewaysOffset = FVector::ZeroVector;
    if (HorizontalJitter > 0.f)
    {
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            const FVector JitterDir = PC->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::RightVector);
            const float JitterAmount = FMath::Sin(Age * 10.f) * HorizontalJitter;
            SidewaysOffset = JitterDir * JitterAmount;
        }
    }

    // 최종 위치 업데이트: 기준 위치 + 위로 상승 + 카메라 기준 좌우 흔들림
    FVector FinalLoc = BaseLoc + (FVector::UpVector * UpValue) + SidewaysOffset;
    SetActorLocation(FinalLoc);

    // 스케일 팝 → 서서히 축소
    float Scale = StartScale;
    if (ScaleCurve)
    {
        Scale = ScaleCurve->GetFloatValue(T);
    }
    else
    {
        const float PopDur = 0.15f;
        if (Age <= PopDur)
        {
            const float p = FMath::Clamp(Age / PopDur, 0.f, 1.f);
            Scale = FMath::Lerp(StartScale, StartScale * 1.2f, FMath::InterpEaseOut(0.f, 1.f, p, 2.f));
        }
        else
        {
            const float p = FMath::Clamp((Age - PopDur) / FMath::Max(0.01f, LifeTime - PopDur), 0.f, 1.f);
            Scale = FMath::Lerp(StartScale * 1.2f, EndScale, p);
        }
    }
    SetActorScale3D(FVector(Scale));

    // 페이드 아웃
    if (UUserWidget* W = WidgetComp->GetUserWidgetObject())
    {
        W->SetRenderOpacity(1.f - T);
    }
}
