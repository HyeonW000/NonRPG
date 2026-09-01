// DamageNumberActor.cpp

#include "Effects/DamageNumberActor.h"
#include "Components/WidgetComponent.h"
#include "UI/DamageNumberWidget.h"
#include "Character/NonCharacterBase.h"
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

    // 🌟 로컬 플레이어 관련 데미지인지 정확히 판별
    APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* LocalPawn = LocalPC ? LocalPC->GetPawn() : nullptr;

    bool bIsMyDamage = true; // 몬스터에게 가한 타격 데미지는 기본적으로 내 데미지(크게)로 표시

    // 만약 피격 대상이 다른 플레이어 캐릭터라면(타인이 맞은 피해) 작게 표시
    if (GetOwner() && GetOwner()->IsA<APawn>() && GetOwner() != LocalPawn)
    {
        if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(GetOwner()))
        {
            if (NonChar->IsPlayerControlled())
            {
                bIsMyDamage = false; // 다른 플레이어가 맞은 피해
            }
        }
    }

    // 내 데미지 vs 다른 파티원/타인의 데미지 (에디터 프로퍼티 연동)
    int32 FontSize = bIsMyDamage
        ? (bInCritical ? MyCritFontSize : MyNormalFontSize)
        : (bInCritical ? OtherCritFontSize : OtherNormalFontSize);

    PendingFontSize = FontSize;

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        W->SetupNumber(PendingAmount, PendingType, PendingFontSize);
    }
}

void ADamageNumberActor::Init(float InDamage, ENonDamageNumberCategory InCategory, int32 InFontSize)
{
    PendingAmount = InDamage;
    PendingType = InCategory;

    // 🌟 로컬 플레이어 관련 데미지인지 정확히 판별
    APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* LocalPawn = LocalPC ? LocalPC->GetPawn() : nullptr;

    bool bIsMyDamage = true;

    if (GetOwner() && GetOwner()->IsA<APawn>() && GetOwner() != LocalPawn)
    {
        if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(GetOwner()))
        {
            if (NonChar->IsPlayerControlled())
            {
                bIsMyDamage = false;
            }
        }
    }

    int32 FinalFontSize = bIsMyDamage ? MyNormalFontSize : OtherNormalFontSize;
    if (InFontSize != 28) // 외부에서 특수한 폰트 크기(예: 무적/회피 등)를 넘겨줬을 경우 비율 축소
    {
        FinalFontSize = bIsMyDamage ? InFontSize : FMath::Max(14, FMath::RoundToInt(InFontSize * 0.55f));
    }

    PendingFontSize = FinalFontSize;

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
        W->SetupLabel(FText::FromString(TEXT("회피")), ENonDamageNumberCategory::Dodge);
    }
}

void ADamageNumberActor::SetupAsImmune()
{
    bIsDodge = true;

    if (UDamageNumberWidget* W = GetDNWidget(WidgetComp))
    {
        W->SetupLabel(FText::FromString(TEXT("무적")), ENonDamageNumberCategory::Dodge);
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
