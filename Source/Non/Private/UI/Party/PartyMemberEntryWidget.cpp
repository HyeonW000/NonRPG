#include "UI/Party/PartyMemberEntryWidget.h"
#include "Core/NonUIManagerComponent.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"

UPartyMemberEntryWidget::UPartyMemberEntryWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UPartyMemberEntryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!bTreeBuilt && (!ProgressBar_HP || !Text_PlayerName))
    {
        BuildCustomWidgetTree();
    }
}

void UPartyMemberEntryWidget::BuildCustomWidgetTree()
{
    bTreeBuilt = true;

    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }

    // 1. 최외곽 카드 보더 (반투명 어두운 배경)
    Border_Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_Card"));
    Border_Card->SetBrushColor(FLinearColor(0.03f, 0.04f, 0.07f, 0.85f));
    Border_Card->SetPadding(FMargin(6.f, 5.f, 8.f, 5.f));
    WidgetTree->RootWidget = Border_Card;

    // 2. 전체 가로 레이아웃: [좌측: 직업 아이콘] + [우측: 이름 & HP/MP 바]
    UHorizontalBox* HBox_Main = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HBox_Main"));
    Border_Card->AddChild(HBox_Main);

    // ── 좌측: 직업 아이콘 영역 (28x28) ──
    USizeBox* SizeBox_Icon = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_Icon"));
    SizeBox_Icon->SetWidthOverride(28.f);
    SizeBox_Icon->SetHeightOverride(28.f);
    if (UHorizontalBoxSlot* HSlot = HBox_Main->AddChildToHorizontalBox(SizeBox_Icon))
    {
        HSlot->SetVerticalAlignment(VAlign_Center);
        HSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
    }

    UBorder* Border_IconBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_IconBg"));
    Border_IconBg->SetBrushColor(FLinearColor(0.08f, 0.1f, 0.14f, 0.95f));
    Border_IconBg->SetPadding(FMargin(2.f));
    SizeBox_Icon->AddChild(Border_IconBg);

    Image_JobIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_JobIcon"));
    Border_IconBg->AddChild(Image_JobIcon);

    // ── 우측: 세로 정보 영역 (이름행 -> HP바 -> MP바) ──
    UVerticalBox* VBox_Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox_Info"));
    if (UHorizontalBoxSlot* HSlot = HBox_Main->AddChildToHorizontalBox(VBox_Info))
    {
        HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        HSlot->SetVerticalAlignment(VAlign_Center);
    }

    // 1행: [★ 파티장] 닉네임
    UHorizontalBox* HBox_Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HBox_Header"));
    if (UVerticalBoxSlot* VSlot = VBox_Info->AddChildToVerticalBox(HBox_Header))
    {
        VSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 3.f));
    }

    // 파티장 별 마크
    Text_LeaderBadge = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_LeaderBadge"));
    Text_LeaderBadge->SetText(FText::FromString(TEXT("★ ")));
    Text_LeaderBadge->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.15f, 1.0f))); // 골드
    FSlateFontInfo LeaderFont = Text_LeaderBadge->GetFont();
    LeaderFont.Size = 11;
    Text_LeaderBadge->SetFont(LeaderFont);
    Text_LeaderBadge->SetVisibility(ESlateVisibility::Collapsed);
    HBox_Header->AddChildToHorizontalBox(Text_LeaderBadge);

    // 닉네임
    Text_PlayerName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_PlayerName"));
    Text_PlayerName->SetText(FText::FromString(TEXT("플레이어")));
    Text_PlayerName->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    FSlateFontInfo NameFont = Text_PlayerName->GetFont();
    NameFont.Size = 11;
    Text_PlayerName->SetFont(NameFont);
    if (UHorizontalBoxSlot* HSlot = HBox_Header->AddChildToHorizontalBox(Text_PlayerName))
    {
        HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    // 2행: HP 바 (높이 8px, 수치 텍스트 없이 깔끔한 게이지)
    USizeBox* SizeBox_HP = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_HP"));
    SizeBox_HP->SetHeightOverride(8.f);
    if (UVerticalBoxSlot* VSlot = VBox_Info->AddChildToVerticalBox(SizeBox_HP))
    {
        VSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
    }

    ProgressBar_HP = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar_HP"));
    ProgressBar_HP->SetFillColorAndOpacity(FLinearColor(0.85f, 0.2f, 0.2f, 1.0f)); // 레드/버건디
    ProgressBar_HP->SetPercent(1.0f);

    FProgressBarStyle HPStyle = ProgressBar_HP->GetWidgetStyle();
    HPStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.1f, 0.1f, 0.12f, 0.9f));
    ProgressBar_HP->SetWidgetStyle(HPStyle);
    SizeBox_HP->AddChild(ProgressBar_HP);

    // 3행: MP 바 (높이 5px, 수치 텍스트 없이 깔끔한 게이지)
    USizeBox* SizeBox_MP = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_MP"));
    SizeBox_MP->SetHeightOverride(5.f);
    VBox_Info->AddChildToVerticalBox(SizeBox_MP);

    ProgressBar_MP = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar_MP"));
    ProgressBar_MP->SetFillColorAndOpacity(FLinearColor(0.22f, 0.58f, 0.98f, 1.0f)); // 산뜻한 블루
    ProgressBar_MP->SetPercent(1.0f);

    FProgressBarStyle MPStyle = ProgressBar_MP->GetWidgetStyle();
    MPStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.1f, 0.1f, 0.12f, 0.9f));
    ProgressBar_MP->SetWidgetStyle(MPStyle);
    SizeBox_MP->AddChild(ProgressBar_MP);
}

void UPartyMemberEntryWidget::GetJobDisplayInfo(EJobClass Job, FString& OutJobName, FLinearColor& OutBadgeColor)
{
    switch (Job)
    {
    case EJobClass::Defender:
        OutJobName = TEXT("수호자");
        OutBadgeColor = FLinearColor(0.35f, 0.72f, 1.0f, 1.0f); // 수호 - 청색
        break;
    case EJobClass::Berserker:
        OutJobName = TEXT("광전사");
        OutBadgeColor = FLinearColor(1.0f, 0.35f, 0.35f, 1.0f); // 버서커 - 붉은색
        break;
    case EJobClass::Cleric:
        OutJobName = TEXT("사제");
        OutBadgeColor = FLinearColor(0.4f, 0.95f, 0.65f, 1.0f); // 클레릭 - 에메랄드 그린
        break;
    case EJobClass::Sorcerer:
        OutJobName = TEXT("마법사");
        OutBadgeColor = FLinearColor(0.85f, 0.45f, 1.0f, 1.0f); // 소서러 - 퍼플
        break;
    default:
        OutJobName = TEXT("모험가");
        OutBadgeColor = FLinearColor(0.75f, 0.75f, 0.75f, 1.0f); // 기본 - 연회색
        break;
    }
}

void UPartyMemberEntryWidget::UpdateMemberInfo(const FPartyMemberInfo& InMemberInfo)
{
    if (!bTreeBuilt && (!ProgressBar_HP || !Text_PlayerName))
    {
        BuildCustomWidgetTree();
    }

    // 1. 파티장 배지
    if (Text_LeaderBadge)
    {
        Text_LeaderBadge->SetVisibility(InMemberInfo.bIsLeader ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    // 2. 직업 아이콘 설정
    if (Image_JobIcon)
    {
        FString JobName;
        FLinearColor JobColor;
        GetJobDisplayInfo(InMemberInfo.JobClass, JobName, JobColor);

        UTexture2D* JobTexture = nullptr;
        if (APlayerController* PC = GetOwningPlayer())
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                if (UNonUIManagerComponent* UIMgr = Pawn->FindComponentByClass<UNonUIManagerComponent>())
                {
                    JobTexture = UIMgr->GetClassIcon(InMemberInfo.JobClass);
                }
            }
        }

        if (JobTexture)
        {
            Image_JobIcon->SetBrushFromTexture(JobTexture);
            Image_JobIcon->SetColorAndOpacity(FLinearColor::White);
        }
        else
        {
            // 아이콘 텍스처가 아직 없을 때는 직업별 고유 색상 브러시로 표시
            Image_JobIcon->SetColorAndOpacity(JobColor);
        }
    }

    // 3. 닉네임
    if (Text_PlayerName)
    {
        Text_PlayerName->SetText(FText::FromString(InMemberInfo.Nickname));
    }

    // 4. HP 프로그레스 바 (게이지 전용)
    const float SafeMaxHP = FMath::Max(1.0f, InMemberInfo.MaxHP);
    const float ClampedHP = FMath::Clamp(InMemberInfo.CurrentHP, 0.0f, SafeMaxHP);
    const float HPPercent = ClampedHP / SafeMaxHP;

    if (ProgressBar_HP)
    {
        ProgressBar_HP->SetPercent(HPPercent);
    }

    // 5. MP 프로그레스 바 (게이지 전용)
    const float SafeMaxMP = FMath::Max(1.0f, InMemberInfo.MaxMP);
    const float ClampedMP = FMath::Clamp(InMemberInfo.CurrentMP, 0.0f, SafeMaxMP);
    const float MPPercent = ClampedMP / SafeMaxMP;

    if (ProgressBar_MP)
    {
        ProgressBar_MP->SetPercent(MPPercent);
    }
}
