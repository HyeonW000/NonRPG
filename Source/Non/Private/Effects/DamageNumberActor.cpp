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

void ADamageNumberActor::SetupNumeric(float InValue, bool bCritical, const FLinearColor& InColor /*= FLinearColor::Transparent*/)
{
    bIsDodge = false;

    // InColor가 투명(기본값)이면 BP 기본색 사용, 아니면 넘겨준 색 우선
    const bool bOverride = (InColor.A >= 0.f) && (InColor != FLinearColor::Transparent);
    const FLinearColor UseColor = bOverride ? InColor : (bCritical ? CriticalDamageColor : NormalDamageColor);

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
        const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
        const FRotator Look = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), Eye);
        SetActorRotation(Look);
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
    // 위로 상승 + (옵션) 좌우 흔들림
    const float Up = RiseSpeed * Age;
    const float XJitter = (HorizontalJitter > 0.f) ? FMath::Sin(Age * 10.f) * HorizontalJitter : 0.f;
    SetActorLocation(BaseLoc + FVector(XJitter, 0.f, Up));

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
