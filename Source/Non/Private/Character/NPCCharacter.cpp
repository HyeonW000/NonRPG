#include "Character/NPCCharacter.h"
#include "Character/NonCharacterBase.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/NonUIManagerComponent.h"

ANPCCharacter::ANPCCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // 상호작용 충돌체 생성 및 설정
    InteractCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractCollision"));
    InteractCollision->SetupAttachment(RootComponent);
    InteractCollision->SetSphereRadius(250.f); // 플레이어가 다가와야 하는 반경
    
    // [Fix] 플레이어의 상호작용 레이저(ECC_GameTraceChannel1)를 막아야 네모난 프롬프트가 뜹니다!
    InteractCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block); // 상호작용 채널 블록

    // 이름 위젯 설정
    NameWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameWidget"));
    NameWidget->SetupAttachment(RootComponent);
    NameWidget->SetWidgetSpace(EWidgetSpace::Screen); // 항상 카메라를 바라보게 (Screen Space)
    NameWidget->SetDrawAtDesiredSize(true);
    NameWidget->SetRelativeLocation(FVector(0.f, 0.f, 100.f)); // 머리 위쪽으로 띄움

    // 기본 이름 및 텍스트 설정
    NPCName = FText::FromString(TEXT("Unknown NPC"));
    DefaultInteractLabel = FText::FromString(TEXT("Talk"));
}

#include "Components/TextBlock.h"

void ANPCCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 블루프린트 위젯(WBP_Name) 안의 텍스트 블록에 NPCName 변수값 강제 주입
    if (NameWidget)
    {
        NameWidget->InitWidget(); // 위젯이 아직 생성되지 않았다면 강제로 즉시 생성!
        if (UUserWidget* WidgetObj = NameWidget->GetUserWidgetObject())
        {
            // 위젯 안에 정확히 "Text_NPCName" 이라는 이름의 TextBlock이 있어야 합니다!
            if (UTextBlock* NameText = Cast<UTextBlock>(WidgetObj->GetWidgetFromName(TEXT("Text_NPCName"))))
            {
                NameText->SetText(NPCName);
            }
        }
    }
}

void ANPCCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // [Improvement] MMO 스타일 네임텍 처리 (부드러운 페이드 + 얇은 장애물 무시)
    if (NameWidget && GetWorld())
    {
        if (APlayerCameraManager* CamManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
        {
            FVector CamLoc = CamManager->GetCameraLocation();
            float Distance = FVector::Dist(CamLoc, GetActorLocation());
            
            bool bShouldShow = true;

            // 1. 거리 체크 (에디터에서 설정한 NameVisibilityDistance 이상 멀어지면 숨김 대상)
            if (Distance > NameVisibilityDistance)
            {
                bShouldShow = false;
            }
            else
            {
                // 2. 스피어 트레이스 (두꺼운 레이저)를 쏴서 얇은 나뭇가지/기둥은 무시하도록 함
                FHitResult Hit;
                FCollisionQueryParams Params(SCENE_QUERY_STAT(NameOcclusion), false, this);
                
                // 반지름 15cm짜리 구체로 스윕하여 얇은 장애물은 빗겨가게 만듭니다.
                FCollisionShape SphereShape = FCollisionShape::MakeSphere(15.f);

                bool bHit = GetWorld()->SweepSingleByChannel(
                    Hit,
                    NameWidget->GetComponentLocation(),
                    CamLoc,
                    FQuat::Identity,
                    ECC_Visibility,
                    SphereShape,
                    Params
                );

                // 플레이어나 카메라 영역이 아닌 진짜 큰 벽에 막혔을 때만 숨김
                if (bHit && Hit.GetActor() && !Hit.GetActor()->IsA(APawn::StaticClass()))
                {
                    bShouldShow = false;
                }
            }

            // 3. 확 나타나고 사라지는 것을 방지하기 위해 투명도(Opacity)를 부드럽게 보간(Fade)
            float TargetOpacity = bShouldShow ? 1.0f : 0.0f;
            
            // UWidgetComponent 내부의 실제 UI 위젯을 가져옵니다.
            if (UUserWidget* InnerWidget = NameWidget->GetUserWidgetObject())
            {
                float CurrentOpacity = InnerWidget->GetRenderOpacity();
                
                // FInterpTo를 사용하면 0초만에 사라지지 않고 NameFadeSpeed의 속도로 스르륵 사라지고 나타납니다.
                float NewOpacity = FMath::FInterpTo(CurrentOpacity, TargetOpacity, DeltaTime, NameFadeSpeed);
                
                InnerWidget->SetRenderOpacity(NewOpacity);

                // 4. [추가] 멀어질수록 글씨 크기를 자연스럽게 줄여줌 (가까울 때 1.0배 -> 멀 때 0.5배)
                float Scale = FMath::GetMappedRangeValueClamped(
                    FVector2D(500.f, NameVisibilityDistance), // 거리 기준 (500보다 가까우면 최대, 설정거리면 최소)
                    FVector2D(1.0f, 0.5f),                    // 스케일 기준 (최대 1.0배 ~ 최소 0.5배)
                    Distance
                );
                InnerWidget->SetRenderScale(FVector2D(Scale, Scale));

                if (NewOpacity < 0.01f && NameWidget->IsVisible())
                {
                    NameWidget->SetVisibility(false);
                }
                else if (NewOpacity >= 0.01f && !NameWidget->IsVisible())
                {
                    NameWidget->SetVisibility(true);
                }
            }
        }
    }

    // [New] 부드러운 회전 처리
    if (bIsRotatingToPlayer)
    {
        FRotator CurrentRot = GetActorRotation();
        FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRotation, DeltaTime, RotationInterpSpeed);
        SetActorRotation(NewRot);

        if (CurrentRot.Equals(TargetRotation, 1.f))
        {
            bIsRotatingToPlayer = false;
        }
    }
}

void ANPCCharacter::Interact_Implementation(ANonCharacterBase* Interactor)
{
    if (Interactor)
    {
        // 1. 플레이어 쪽으로 몸을 돌림
        FaceInteractor(Interactor);

        // [New] 2. 오버 숄더 카메라 연출 시작
        Interactor->BeginDialogueCamera(this);

        // 3. 역할에 따른 기능 분기
        FString RoleString = TEXT("일반");
        switch (NPCRole)
        {
        case ENPCRole::Merchant:    RoleString = TEXT("상점 열기"); break;
        case ENPCRole::Blacksmith:  RoleString = TEXT("대장간 열기"); break;
        case ENPCRole::QuestGiver:  RoleString = TEXT("퀘스트 확인"); break;
        }

        // 4. 대화 UI 표시
        if (UNonUIManagerComponent* UIManager = Interactor->FindComponentByClass<UNonUIManagerComponent>())
        {
            if (DialogueTable && !StartingDialogueID.IsNone())
            {
                // [New] 데이터 테이블을 이용한 본격적인 대화/선택지 시스템 시작
                UIManager->StartDialogue(DialogueTable, StartingDialogueID, NPCName);
            }
            else
            {
                // [기존] 대화 테이블이 안 꽂혀있으면 임시 인사말 출력
                FString DialogueMessage = FString::Printf(TEXT("반갑소, %s! [%s] 기능은 아직 준비 중이오."), *Interactor->GetPlayerName(), *RoleString);
                UIManager->ShowDialogue(NPCName, FText::FromString(DialogueMessage));
            }
        }
    }
}

FText ANPCCharacter::GetInteractLabel_Implementation()
{
    return DefaultInteractLabel; // "대화하기" 등의 텍스트 반환
}

void ANPCCharacter::SetInteractHighlight_Implementation(bool bEnable)
{
    // NPC 외곽선(아웃라인) On/Off
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        // 프로젝트에 CustomDepth로 아웃라인을 그리고 있다면 이를 켭니다.
        MeshComp->SetRenderCustomDepth(bEnable);
        MeshComp->SetCustomDepthStencilValue(1); // 1번 스텐실(하이라이트 색상)
    }
}

void ANPCCharacter::FaceInteractor(AActor* Interactor)
{
    if (!Interactor) return;

    // 플레이어를 향하는 회전값 계산 (Z축 회전만 적용하여 고개만 숙이지 않게 함)
    FVector Direction = Interactor->GetActorLocation() - GetActorLocation();
    Direction.Z = 0.f; // 높이차 무시
    
    TargetRotation = FRotationMatrix::MakeFromX(Direction).Rotator();
    bIsRotatingToPlayer = true;
}
