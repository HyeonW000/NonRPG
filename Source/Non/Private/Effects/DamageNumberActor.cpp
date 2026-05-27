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

    const ENonDamageNumberCategory UseCategory = bInCritical ? ENonDamageNumberCategory::Critical : ENonDamageNumberCategory::Normal;

    PendingAmount = InAmount;
    PendingType = UseCategory;

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        W->SetupNumber(PendingAmount, PendingType);
    }
}

void ADamageNumberActor::Init(float InDamage, ENonDamageNumberCategory InCategory, int32 InFontSize)
{
    PendingAmount = InDamage;
    PendingFontSize = InFontSize;
    PendingType = InCategory;

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        W->SetupNumber(PendingAmount, PendingType, PendingFontSize);
    }
}

void ADamageNumberActor::InitAsLabel(const FText& InText, ENonDamageNumberCategory InCategory, int32 InFontSize)
{
    bPendingLabel = true;
    PendingLabel = InText;
    PendingFontSize = InFontSize;
    PendingType = InCategory;

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        W->SetupLabel(PendingLabel, PendingType, PendingFontSize);
        bPendingLabel = false;
    }
}

void ADamageNumberActor::SetupNumeric(float InValue, bool bCritical, const FLinearColor& /*InColor*/)
{
    bIsDodge = false;

    const ENonDamageNumberCategory UseCategory = bCritical ? ENonDamageNumberCategory::Critical : ENonDamageNumberCategory::Normal;

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        W->SetupNumber(InValue, UseCategory);
    }
}

void ADamageNumberActor::SetupAsDodge()
{
    bIsDodge = true;

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        W->SetupLabel(FText::FromString(TEXT("Dodge")), ENonDamageNumberCategory::Dodge);
    }
}

void ADamageNumberActor::BeginPlay()
{
    Super::BeginPlay();

    if (!WidgetComp->GetUserWidgetObject())
    {
        WidgetComp->InitWidget();
    }

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        if (PendingAmount != 0.f)
        {
            W->SetupNumber(PendingAmount, PendingType, PendingFontSize);
        }

        if (bPendingLabel)
        {
            W->SetupLabel(PendingLabel, PendingType, PendingFontSize);
            bPendingLabel = false;
        }
    }

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            WidgetComp->SetOwnerPlayer(LP);
        }
        
        WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
        WidgetComp->SetDrawAtDesiredSize(true);
        WidgetComp->SetTickWhenOffscreen(true);
        WidgetComp->SetBoundsScale(10.0f);

        const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
        const FRotator Look = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), Eye);
        SetActorRotation(Look);
    }
}

void ADamageNumberActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    Age += DeltaSeconds;

    if (Age >= LifeTime)
    {
        Destroy();
    }
}
