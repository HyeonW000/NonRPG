#include "Character/NonCharacterBase.h"
#include "Core/NonGameplayTags.h"
#include "Ability/GA_ComboBase.h"
#include "Ability/NonAbilitySystemComponent.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "UI/QuickSlot/QuickSlotManager.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Effects/JustDodgeAfterimage.h"
#include "Character/BossCharacter.h"
#include "Materials/MaterialInterface.h"

#include "Camera/CameraComponent.h"
#include "InputActionValue.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Core/NonPlayerController.h"
#include "GameFramework/SpringArmComponent.h"

#include "Character/EnemyCharacter.h"
#include "Character/NPCCharacter.h"
#include "DrawDebugHelpers.h"

#include "Core/NonUIManagerComponent.h"
#include "Camera/CameraActor.h"
#include "Data/LevelData.h"
#include "Effects/DamageNumberActor.h"
#include "Equipment/EquipmentComponent.h"
#include "Ability/Skills/GA_SkillBase.h"
#include "Inventory/InventoryComponent.h" // [Fix] Moved to global scope
#include "Inventory/InventoryItem.h"
#include "Combat/CombatSlotComponent.h"
#include "Character/NonDeathComponent.h"
#include "Ability/GA_ConsumePotion.h"
#include "System/SaveGameSubsystem.h"

#include "Animation/AnimInstance.h"
#include "GameFramework/GameStateBase.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSetTypes.h"    // ← EWeaponStance 등
#include "Animation/NonAnimInstance.h" // ← AnimBP 경유
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h" // [New] for DOREPLIFETIME

#include "AbilitySystemBlueprintLibrary.h"
#include "Skill/SkillManagerComponent.h"
#include "Skill/SkillTypes.h"
#include "Interaction/NonInteractableInterface.h"
#include "MotionWarpingComponent.h"
#include "GameFramework/RootMotionSource.h"

void ANonCharacterBase::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty> &OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);

  // Character
  DOREPLIFETIME(ANonCharacterBase, bIsBackpedaling);
  DOREPLIFETIME(ANonCharacterBase, bForceFullBody);
  DOREPLIFETIME(ANonCharacterBase, bStrafeMode);
  DOREPLIFETIME(ANonCharacterBase, DefaultJobClass);
  DOREPLIFETIME(ANonCharacterBase, DeathServerTime);
  DOREPLIFETIME(ANonCharacterBase, PlayerName);
  DOREPLIFETIME_CONDITION(ANonCharacterBase, bArmed, COND_SkipOwner);
  DOREPLIFETIME_CONDITION(ANonCharacterBase, bDrawSheatheMoving, COND_SkipOwner);
}

void ANonCharacterBase::OnRep_JobClass() {
  // UI 갱신 필요 시 여기서 Delegate Broadcast
}

ANonCharacterBase::ANonCharacterBase() {
  PrimaryActorTick.bCanEverTick = true;

  // GAS
  AbilitySystemComponent = CreateDefaultSubobject<UNonAbilitySystemComponent>(
      TEXT("AbilitySystemComponent"));
  AbilitySystemComponent->SetIsReplicated(true);
  AbilitySystemComponent->SetReplicationMode(
      EGameplayEffectReplicationMode::Mixed);

  // [Fix] 공격 속도 빠른 게임이므로 업데이트 빈도 상향 (기본 33 -> 100)
  SetNetUpdateFrequency(100.f);
  SetMinNetUpdateFrequency(33.f);

  // 태그별 몽타주 매핑 초기화

  AttributeSet = CreateDefaultSubobject<UNonAttributeSet>(TEXT("AttributeSet"));

  // 카메라
  CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
  CameraBoom->SetupAttachment(RootComponent);
  CameraBoom->TargetArmLength = 400.0f;
  CameraBoom->bUsePawnControlRotation = true;

  FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
  FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
  FollowCamera->bUsePawnControlRotation = false;

  // 머리 위 이름표 위젯 컴포넌트
  OverheadNameWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadNameWidget"));
  OverheadNameWidget->SetupAttachment(RootComponent);
  OverheadNameWidget->SetWidgetSpace(EWidgetSpace::Screen);
  OverheadNameWidget->SetDrawAtDesiredSize(true);
  OverheadNameWidget->SetRelativeLocation(FVector(0.f, 0.f, 105.f));

  // --- Modular Parts Init (다시 복구) ---
  // 헤더 파일에는 선언이 남아있으므로, CPP에서도 초기화를 해야 에러가 안
  // 납니다.
  HeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadMesh"));

  HairMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HairMesh"));
  EyebrowsMesh =
      CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EyebrowsMesh"));

  // Master Pose (GetMesh가 움직이면 따라 움직임)
  HeadMesh->SetupAttachment(GetMesh());
  // HeadMesh->SetLeaderPoseComponent(GetMesh());

  HairMesh->SetupAttachment(HeadMesh);
  EyebrowsMesh->SetupAttachment(HeadMesh);

  // HeadMesh가 얼굴 표정(Morph) 등을 담당한다면,
  // 눈썹(Eyebrows)과 머리카락(Hair)도 그 변형을 따라가야 하므로 LeaderPose를
  // Head로 설정합니다.
  HairMesh->SetLeaderPoseComponent(HeadMesh);
  EyebrowsMesh->SetLeaderPoseComponent(HeadMesh);

  // [Fix] 모듈러 파츠가 줌인 시 컬링되지 않도록 바운드 스케일 대폭
  // 확장 1.5f 로도 부족하다면 더 크게(3.0 ~ 5.0) 설정하여 확실하게 컬링 방지
  constexpr float ModularBoundsScale = 5.0f;
  HeadMesh->SetBoundsScale(ModularBoundsScale);
  HairMesh->SetBoundsScale(ModularBoundsScale);
  EyebrowsMesh->SetBoundsScale(ModularBoundsScale);

  bUseControllerRotationYaw = false;
  GetCharacterMovement()->bOrientRotationToMovement = true;

  // [Multiplayer Fix] 다른 플레이어가 내 캡슐/메쉬를 계단처럼 밟고 위로 올라타지 못하도록 완전 차단
  if (GetCapsuleComponent()) {
    GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
  }

  if (GetMesh()) {
    GetMesh()->CanCharacterStepUpOn = ECB_No;
    GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
  }

  if (HeadMesh) HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  if (HairMesh) HairMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  if (EyebrowsMesh) EyebrowsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

  UIManagerComponent =
      CreateDefaultSubobject<UNonUIManagerComponent>(TEXT("UIManagerComponent"));
  InventoryComp = CreateDefaultSubobject<UInventoryComponent>(
      TEXT("InventoryComp")); // [Fix] Create Subobject
  QuickSlotManager =
      CreateDefaultSubobject<UQuickSlotManager>(TEXT("QuickSlotManager"));
  CombatSlotComponent =
      CreateDefaultSubobject<UCombatSlotComponent>(TEXT("CombatSlotComponent"));
  MotionWarpingComp = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComp"));
  GuardComponent = CreateDefaultSubobject<UNonGuardComponent>(TEXT("GuardComponent"));
  DeathComponent = CreateDefaultSubobject<UNonDeathComponent>(TEXT("DeathComponent"));

  // ── [Interaction] 상호작용 충돌체 생성 및 설정 (NPC와 동일하게 세팅) ──
  InteractCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractCollision"));
  InteractCollision->SetupAttachment(RootComponent);
  InteractCollision->SetSphereRadius(55.f);
  InteractCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  InteractCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
  InteractCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
  InteractCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);

  // [Changed] 무기 들었을(StrafeMode) 때는 움직이지 않아도 항상 카메라 방향
  // 보기
  bOnlyFollowCameraWhenMoving = false;

  // [Multiplayer Fix] 서버에서도 애니메이션 노티파이(소켓 스왑 등)가 정상
  // 실행되도록 틱 옵션 설정 기본값 최적화로 인해 서버에서 노티파이가 씹히는
  // 현상 방지
  GetMesh()->VisibilityBasedAnimTickOption =
      EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void ANonCharacterBase::BeginPlay() {
  Super::BeginPlay();

  // [Fix] 상호작용 채널(ECC_GameTraceChannel1) Block 강제 보장 (블루프린트 직렬화 값 덮어쓰기 방지)
  if (GetCapsuleComponent()) {
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
  }
  if (InteractCollision) {
    InteractCollision->SetSphereRadius(50.f); // [Fix] 블루프린트 에셋에 남아있는 250cm 구체를 50cm로 강제 덮어쓰기
    InteractCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
  }

  // [Fix] 디폴트 걷기 속도 캡처 (이동 속도 복원용)
  if (UCharacterMovementComponent *Move = GetCharacterMovement()) {
    WalkSpeed_Default = Move->MaxWalkSpeed;
  }

  // [Multiplayer Test Fix] BegiPlay 최상단으로 이동
  // Play as Client 등의 환경에서는 SaveGame 로드가 서버에서 실패(파일
  // 없음)하거나, 클라이언트의 선택 정보가 넘어오지 않아 기본값(None) 상태일 수
  // 있습니다. 테스트를 위해 권한(Authority)이 있을 때 강제로 초기화를
  // 수행합니다.
  if (HasAuthority()) {
    // [Logic Fix] DefaultJobClass가 None일때만 Defender로 세팅. 이미 값이
    // 있다면(BP 설정값) 유지
    if (DefaultJobClass == EJobClass::None) {
      DefaultJobClass = EJobClass::Defender;
    }

    // 1. HP/Mana 등 스탯 초기화 (이게 없으면 사망 처리됨)
    InitializeAttributes();

    // 2. 장비/직업 데이터 초기화
    InitCharacterData(DefaultJobClass, 1);
  }

  // ASC 캐싱
  if (IAbilitySystemInterface *ASI = Cast<IAbilitySystemInterface>(this)) {
    ASC = ASI->GetAbilitySystemComponent();
  }
  if (!ASC)
    ASC = FindComponentByClass<UAbilitySystemComponent>();

  if (ASC) {
    // [Fix] Multi: 모든 클라이언트(Local/Proxy) 및 서버에서 ActorInfo 초기화가
    // 필요함. PossessedBy에서만 호출하면 클라이언트가 초기화되지 않아 스탯
    // 리플리케이션을 못 받을 수 있음.
    ASC->InitAbilityActorInfo(this, this);

    // C++ 레벨 디버그 델리게이트 바인딩
    BindASCDelegates();
  }

  // 다른 플레이어 화면에서 발도·납도 몽타주의 루트 이동을 시작 즉시 끄기 위한 바인딩
  if (USkeletalMeshComponent *MeshForMontage = GetMesh()) {
    if (UAnimInstance *AnimForMontage = MeshForMontage->GetAnimInstance()) {
      AnimForMontage->OnMontageStarted.AddUniqueDynamic(this, &ANonCharacterBase::HandleMontageStartedForProxyRootMotion);
    }
  }

  // 장착/이동 초기화(네 기존 코드 유지)
  CachedArmedStance = DefaultArmedStance;
  SyncEquippedStanceFromEquipment();
  if (UCharacterMovementComponent *Move = GetCharacterMovement()) {
    WalkSpeed_Default = Move->MaxWalkSpeed;
  }
  RefreshWeaponStance();
  UpdateStrafeYawFollowBySpeed();

  // ── [Combat|Guard|Turn] 가드 제자리 턴 몽타주: BP_NonCharacterBase에서 지정 (비어 있으면 턴 없이 몸만 회전) ──
  if (!GuardTurnLeft90Montage || !GuardTurnRight90Montage || !GuardTurnLeft180Montage || !GuardTurnRight180Montage) {
    static bool bWarnedGuardTurn = false;
    if (!bWarnedGuardTurn) {
      bWarnedGuardTurn = true;
      UE_LOG(LogTemp, Warning, TEXT("[GuardTurn] %s: 가드 턴 몽타주(GuardTurnLeft90/Right90/Left180/Right180Montage) 중 비어 있는 값이 있습니다. BP Class Defaults에서 지정하세요"), *GetClass()->GetName());
    }
  }

  // ── GAS Attribute 변경 델리게이트: UI 업데이트 + 사망 체크를 한 번에 처리 ──
  if (AbilitySystemComponent && AttributeSet) {
    AbilitySystemComponent
        ->GetGameplayAttributeValueChangeDelegate(
            AttributeSet->GetHPAttribute())
        .AddLambda([this](const FOnAttributeChangeData &Data) {
          // UI가 있을 때만 갱신 (나는 내 화면의 UI만 갱신하면 됨)
          if (IsLocallyControlled() && UIManagerComponent) {
            UIManagerComponent->UpdateHP(Data.NewValue, AttributeSet->GetMaxHP());
          }

          // HP 0 → 사망 (서버/클라 모두 체크하거나, 사망 처리는 서버에서만? ->
          // 보통은 서버가 권한을 가짐) 현재 구조는 서버도 HandleDeath 호출 필요
          // (충돌 해제 등) 클라는 서버의 Replicated 상태(bDied)를 받을 수도
          // 있지만, 일단 여기서 로컬 예측 처리 또는 서버직접처리를 위해 모두
          // 허용하되, UI만 거른다.
          if (!bDied && Data.OldValue > 0.f && Data.NewValue <= 0.f) {
            HandleDeath(); // Server calls this -> Replicates bDied -> Clients
                           // handle visual
          }
        });

    AbilitySystemComponent
        ->GetGameplayAttributeValueChangeDelegate(
            AttributeSet->GetMPAttribute())
        .AddLambda([this](const FOnAttributeChangeData &Data) {
          if (IsLocallyControlled() && UIManagerComponent) {
            UIManagerComponent->UpdateMP(Data.NewValue, AttributeSet->GetMaxMP());
          }
        });

    AbilitySystemComponent
        ->GetGameplayAttributeValueChangeDelegate(
            AttributeSet->GetSPAttribute())
        .AddLambda([this](const FOnAttributeChangeData &Data) {
          if (IsLocallyControlled() && UIManagerComponent) {
            UIManagerComponent->UpdateSP(Data.NewValue, AttributeSet->GetMaxSP());
          }
        });

    AbilitySystemComponent
        ->GetGameplayAttributeValueChangeDelegate(
            AttributeSet->GetLevelAttribute())
        .AddLambda([this](const FOnAttributeChangeData &Data) {
          if (IsLocallyControlled() && UIManagerComponent) {
            UIManagerComponent->UpdateLevel(static_cast<int32>(Data.NewValue));
          }
        });

    AbilitySystemComponent
        ->GetGameplayAttributeValueChangeDelegate(
            AttributeSet->GetExpAttribute())
        .AddLambda([this](const FOnAttributeChangeData &Data) {
          if (IsLocallyControlled() && UIManagerComponent) {
            if (Data.NewValue >= AttributeSet->GetExpToNextLevel()) {
              LevelUp();
            }
            UIManagerComponent->UpdateEXP(Data.NewValue,
                                     AttributeSet->GetExpToNextLevel());
          }
        });
  }

  // ── 초기 UI 값 세팅 (플레이어 컨트롤일 때만) ──
  // [Fix] 로컬 플레이어인 경우에만 위젯 업데이트 시도 (서버에서 클라 위젯 접근
  // 금지)
  if (IsLocallyControlled()) {
    if (APlayerController *PC = Cast<APlayerController>(Controller)) {
      if (AbilitySystemComponent && AttributeSet && UIManagerComponent) {
        UIManagerComponent->UpdateHP(AttributeSet->GetHP(),
                                AttributeSet->GetMaxHP());
        UIManagerComponent->UpdateMP(AttributeSet->GetMP(),
                                AttributeSet->GetMaxMP());
        UIManagerComponent->UpdateSP(AttributeSet->GetSP(),
                                AttributeSet->GetMaxSP());
        UIManagerComponent->UpdateEXP(AttributeSet->GetExp(),
                                 AttributeSet->GetExpToNextLevel());
        UIManagerComponent->UpdateLevel(
            static_cast<int32>(AttributeSet->GetLevel()));
      }
    }
  }

  if (!EquipmentComp)
    EquipmentComp = FindComponentByClass<UEquipmentComponent>();

  // 무기 슬롯이 바뀌면 캐스팅 취소 (설계서 무기 교체 규칙)
  if (EquipmentComp) {
    EquipmentComp->OnEquipped.AddUniqueDynamic(this, &ANonCharacterBase::HandleEquipmentEquippedForCasting);
    EquipmentComp->OnUnequipped.AddUniqueDynamic(this, &ANonCharacterBase::HandleEquipmentUnequippedForCasting);
  }

  if (!InventoryComp)
    InventoryComp = FindComponentByClass<UInventoryComponent>();

  if (!InventoryComp) {
    InventoryComp = NewObject<UInventoryComponent>(this, TEXT("InventoryComp_Dynamic"));
    InventoryComp->RegisterComponent();
  }

  // [Item System] 캐릭터 디테일 패널에서 설정한 데이터 테이블을 인벤토리에 안전하게 연결
  if (InventoryComp && DefaultItemDataTable) {
    InventoryComp->ItemDataTable = DefaultItemDataTable;
  }

  // [fix] 클라이언트(Simulated Proxy)는 아직 HP 리플리케이션을 못 받았을 수
  // 있음(0으로 시작). 따라서 서버에서만 체크하거나, bDied 리플리케이션을 믿어야
  // 함.
  if (HasAuthority() && !bDied && AttributeSet &&
      AttributeSet->GetHP() <= 0.f) {
    HandleDeath();
  }

  // === 스킬 매니저 초기화 ===
  if (!SkillMgr)
    SkillMgr = FindComponentByClass<USkillManagerComponent>();

  // 1) 서버/클라 공통: DataAsset / ASC 세팅 (UI에서 CanLevelUp 등에 사용)
  if (SkillMgr && ASC) {
    SkillMgr->Init(DefaultJobClass, nullptr, ASC);
  }

  // 2) 서버 전용: 직업/포인트 설정 (Replicated)
  if (HasAuthority() && SkillMgr) {
    SkillMgr->SetJobClass(DefaultJobClass);
    SkillMgr->AddSkillPoints(0);
  }

  // [Temp OFF for Testing] 게임 시작 시 자동 로드 (테스트를 위해 잠시 OFF)
  /*
  if (USaveGameSubsystem *SaveSys =
          GetGameInstance()->GetSubsystem<USaveGameSubsystem>()) {
    SaveSys->LoadGame();
  }
  */

  // ── 👥 [Multiplayer] 플레이어 캐릭터끼리는 충돌 침투로 인해 위로 붕 뜨지 않도록 완벽 상호 무시 등록 ──
  if (UCapsuleComponent* MyCapsule = GetCapsuleComponent()) {
    MyCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
  }

  TArray<AActor*> FoundPlayers;
  UGameplayStatics::GetAllActorsOfClass(GetWorld(), ANonCharacterBase::StaticClass(), FoundPlayers);
  for (AActor* OtherActor : FoundPlayers) {
    if (OtherActor && OtherActor != this) {
      if (UCapsuleComponent* MyCapsule = GetCapsuleComponent()) {
        MyCapsule->IgnoreActorWhenMoving(OtherActor, true);
      }
      if (ANonCharacterBase* OtherChar = Cast<ANonCharacterBase>(OtherActor)) {
        if (UCapsuleComponent* OtherCapsule = OtherChar->GetCapsuleComponent()) {
          OtherCapsule->IgnoreActorWhenMoving(this, true);
        }
      }
    }
  }

  // 나중에 접속하는 플레이어와의 무시 등록을 위해 1초 뒤 한 번 더 스캔 보장
  FTimerHandle IgnoreTimerHandle;
  GetWorldTimerManager().SetTimer(IgnoreTimerHandle, [this]() {
    if (!IsValid(this)) return;
    TArray<AActor*> Players;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ANonCharacterBase::StaticClass(), Players);
    for (AActor* OtherActor : Players) {
      if (OtherActor && OtherActor != this) {
        if (UCapsuleComponent* MyCap = GetCapsuleComponent()) {
          MyCap->IgnoreActorWhenMoving(OtherActor, true);
        }
        if (ANonCharacterBase* OtherChar = Cast<ANonCharacterBase>(OtherActor)) {
          if (UCapsuleComponent* OtherCap = OtherChar->GetCapsuleComponent()) {
            OtherCap->IgnoreActorWhenMoving(this, true);
          }
        }
      }
    }
  }, 1.0f, false);

  // 머리 위 이름표 갱신
  UpdateOverheadName(PlayerName);
}

void ANonCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason) {
  Super::EndPlay(EndPlayReason);

  // [Temp OFF for Testing] 캐릭터 소멸 시 자동 저장 (테스트를 위해 잠시 OFF)
  /*
  if (UWorld *World = GetWorld()) {
    if (UGameInstance *GI = World->GetGameInstance()) {
      if (USaveGameSubsystem *SaveSys =
              GI->GetSubsystem<USaveGameSubsystem>()) {
        SaveSys->SaveGame();
      }
    }
  }
  */
}

void ANonCharacterBase::ToggleArmed() { SetArmed(!bArmed); }

void ANonCharacterBase::CameraZoom(float Value) {
  if (Value == 0.f || !CameraBoom)
    return;

  // 휠 올리면(Value > 0) 줌인(=길이 감소), 내리면(Value < 0) 줌아웃(=길이 증가)
  // 일반적인 휠 동작: 위로 밀면(+), 아래로 당기면(-)

  // Zoom Step만큼 빼줘야 줌인이 됨
  const float NewLen = CameraBoom->TargetArmLength - (Value * CameraZoomStep);
  CameraBoom->TargetArmLength =
      FMath::Clamp(NewLen, CameraZoomMin, CameraZoomMax);
}

void ANonCharacterBase::ServerAddItem_Implementation(FName ItemId,
                                                     int32 Quantity) {
  // 클라이언트가 원하는 아이템을 만들 수 있는 디버그용 요청 -> 배포 빌드에서는 무시
#if !UE_BUILD_SHIPPING
  if (InventoryComp) {
    int32 Dummy;
    InventoryComp->AddItem(ItemId, Quantity, Dummy);
  }
#else
  UE_LOG(LogTemp, Warning, TEXT("[Inventory] ServerAddItem 거부 (배포 빌드): %s"), *ItemId.ToString());
#endif
}

void ANonCharacterBase::ServerAddMultipleItems_Implementation(const TArray<FName>& ItemIds,
                                                              int32 QuantityPerItem) {
  // 디버그용 요청 -> 배포 빌드에서는 무시
#if !UE_BUILD_SHIPPING
  if (InventoryComp) {
    InventoryComp->AddMultipleItems(ItemIds, QuantityPerItem);
  }
#else
  UE_LOG(LogTemp, Warning, TEXT("[Inventory] ServerAddMultipleItems 거부 (배포 빌드)"));
#endif
}

void ANonCharacterBase::ServerSortInventory_Implementation() {
  if (InventoryComp) {
    InventoryComp->SortInventory();
  }
}

void ANonCharacterBase::SetEquippedStance(EWeaponStance NewStance) {
  CachedArmedStance = NewStance;
}

void ANonCharacterBase::SetArmed(bool bNewArmed) {
  if (bArmed == bNewArmed) {
    return;
  }

  // 1) 무장 플래그 갱신 (서버에서 바뀌면 다른 플레이어에게 복제됨)
  bArmed = bNewArmed;

  ApplyArmedLocalState();
}

void ANonCharacterBase::HandleEquipmentEquippedForCasting(EEquipmentSlot Slot, UInventoryItem * /*Item*/) {
  if (Slot == EEquipmentSlot::WeaponMain || Slot == EEquipmentSlot::WeaponSub) {
    CancelCastingForWeaponChange();
  }
}

void ANonCharacterBase::HandleEquipmentUnequippedForCasting(EEquipmentSlot Slot) {
  if (Slot == EEquipmentSlot::WeaponMain || Slot == EEquipmentSlot::WeaponSub) {
    CancelCastingForWeaponChange();
  }
}

void ANonCharacterBase::CancelCastingForWeaponChange() {
  // 스킬 어빌리티 인스턴스는 서버와 본인 화면에만 있음
  if (!AbilitySystemComponent || !(HasAuthority() || IsLocallyControlled())) {
    return;
  }

  // 취소하면서 목록이 바뀔 수 있으므로 먼저 모아 둔 뒤 취소
  TArray<UGA_SkillBase *> CastingSkills;
  for (const FGameplayAbilitySpec &Spec : AbilitySystemComponent->GetActivatableAbilities()) {
    if (!Spec.IsActive()) {
      continue;
    }
    for (UGameplayAbility *Instance : Spec.GetAbilityInstances()) {
      if (UGA_SkillBase *Skill = Cast<UGA_SkillBase>(Instance)) {
        CastingSkills.Add(Skill);
      }
    }
  }
  for (UGA_SkillBase *Skill : CastingSkills) {
    if (IsValid(Skill)) {
      Skill->CancelCastingForWeaponChange();
    }
  }
}

void ANonCharacterBase::ApplyArmedLocalState() {
  // 설계서 3.7 & 4장: 발도 상태(State.Weapon.Drawn) 태그 동기화
  if (AbilitySystemComponent) {
    static const FGameplayTag Tag_WeaponDrawn =
        NonTags::State_Weapon_Drawn.GetTag();
    if (Tag_WeaponDrawn.IsValid()) {
      AbilitySystemComponent->SetLooseGameplayTagCount(Tag_WeaponDrawn, bArmed ? 1 : 0);
    }
  }

  // 2) 장착된 무기 기준으로 스탠스 맞추기
  SyncEquippedStanceFromEquipment();

  // 3) 스탠스/스트레이프 정리
  RefreshWeaponStance();

  // 4) 어빌리티 시스템 무기 태그 갱신
  if (EquipmentComp) {
    EquipmentComp->UpdateWeaponTags(bArmed);
  }
}

void ANonCharacterBase::OnRep_Armed() {
  ApplyArmedLocalState();
  // 발도/납도 몽타주가 재생 중이면 그 노티파이가 무기를 옮기도록 잠깐 기다림 (최대 약 2초)
  ScheduleArmedWeaponSocketCheck(20);
}

void ANonCharacterBase::ScheduleArmedWeaponSocketCheck(int32 RetriesLeft) {
  const UAnimInstance *AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
  const bool bMontagePlaying = AnimInst && AnimInst->IsAnyMontagePlaying();
  if (!bMontagePlaying || RetriesLeft <= 0) {
    ReattachMainWeaponForArmedState(bArmed);
    return;
  }

  FTimerHandle CheckHandle;
  GetWorldTimerManager().SetTimer(
      CheckHandle,
      FTimerDelegate::CreateWeakLambda(this, [this, RetriesLeft]() {
        ScheduleArmedWeaponSocketCheck(RetriesLeft - 1);
      }),
      0.1f, false);
}

void ANonCharacterBase::ReattachMainWeaponForArmedState(bool bWantArmed) {
  UEquipmentComponent *Eq = EquipmentComp ? EquipmentComp.Get() : FindComponentByClass<UEquipmentComponent>();
  if (!Eq) {
    return;
  }
  UInventoryItem *MainItem = Eq->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain);
  if (!MainItem) {
    return;
  }

  FName DesiredSocket = NAME_None;
  if (bWantArmed) {
    DesiredSocket = GetHandSocketForItem(MainItem);
  } else {
    DesiredSocket = Eq->GetHomeSocketForSlot(EEquipmentSlot::WeaponMain);
    if (DesiredSocket == NAME_None) {
      DesiredSocket = GetHomeFallbackSocketForItem(MainItem);
    }
  }
  if (DesiredSocket == NAME_None) {
    return;
  }

  FName CurrentSocket = NAME_None;
  if (AActor *SpawnedWeapon = Eq->GetEquippedActor(EEquipmentSlot::WeaponMain)) {
    CurrentSocket = SpawnedWeapon->GetAttachParentSocketName();
  } else if (UMeshComponent *MainMesh = Eq->GetVisualComponent(EEquipmentSlot::WeaponMain)) {
    CurrentSocket = MainMesh->GetAttachSocketName();
  } else {
    return;
  }

  if (CurrentSocket != DesiredSocket) {
    Eq->ReattachSlotToSocket(EEquipmentSlot::WeaponMain, DesiredSocket, FTransform::Identity);
  }
}

bool ANonCharacterBase::TryAutoDrawWeapon() {
  if (bArmed) {
    return false; // 이미 발도 상태이므로 정상 행동 진행
  }

  // 주무기가 장착되어 있는지 확인 (설계서 3.7: 무기 미장착 시 발도 불가)
  if (!EquipmentComp) {
    EquipmentComp = FindComponentByClass<UEquipmentComponent>();
  }
  if (!EquipmentComp || !EquipmentComp->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain)) {
    return true; // 무기 미장착 상태이므로 발도/공격 차단
  }

  // 납도 상태에서 공격/방어/스킬 키 입력 시 발도 모션(Ability.ToggleWeapon)만 재생
  if (AbilitySystemComponent) {
    static const FGameplayTag ToggleTag =
        NonTags::Ability_ToggleWeapon.GetTag();
    if (ToggleTag.IsValid()) {
      FGameplayTagContainer TagContainer(ToggleTag);
      AbilitySystemComponent->TryActivateAbilitiesByTag(TagContainer);
    }
  }
  return true; // 이번 입력은 발도만 수행하고 행동은 이어서 실행하지 않음 (입력 버퍼 없음)
}

void ANonCharacterBase::MoveInput(const FInputActionValue &Value) {
  // 대화 중이거나 상점 UI가 활성화된 상황에서만 이동을 차단합니다 (일반 인벤토리 오픈 시에는 차단하지 않습니다).
  if (bIsDialogueCameraActive || (UIManagerComponent && UIManagerComponent->IsMerchantShopOpen())) {
      return;
  }

  // [New] 피격 상태(State.HitReacting)이거나 사망 상태면 이동 입력 무시
  if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(NonTags::State_HitReacting.GetTag())) {
      return;
  }

  FVector2D Movement = Value.Get<FVector2D>();

  if (Controller != nullptr && Movement.SizeSquared() > 0.f) {
    const FRotator Rotation = Controller->GetControlRotation();
    const FRotator YawRotation(0, Rotation.Yaw, 0);

    const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    AddMovementInput(Forward, Movement.Y);
    AddMovementInput(Right, Movement.X);
  }
}

void ANonCharacterBase::SetStrafeMode(bool bEnable) {
  // 이미 같으면 스킵
  if (bStrafeMode == bEnable)
    return;

  bStrafeMode = bEnable;

  // 서버, 클라(LocalPredicted) 모두 즉시 로직 반영을 위해 호출
  OnRep_StrafeMode();
}

void ANonCharacterBase::OnRep_StrafeMode() {
  UCharacterMovementComponent *Move = GetCharacterMovement();
  if (!Move)
    return;

  // Strafe 모드에 따른 회전 세팅 업데이트
  if (bStrafeMode) {
    bUseControllerRotationYaw = false;
    // [Fix] 기본은 false (입력 있을 때만 true로 변경됨 -
    // UpdateStrafeYawFollowBySpeed)
    Move->bUseControllerDesiredRotation = false;
    Move->bOrientRotationToMovement = false;
  } else {
    // 일반 이동 모드 (보통 3인칭 어드벤처)
    bUseControllerRotationYaw = false;
    Move->bUseControllerDesiredRotation = false;
    Move->bOrientRotationToMovement = true;
    Move->RotationRate = FRotator(0.f, 720.f, 0.f); // 빠른 회전
  }
}

void ANonCharacterBase::Tick(float DeltaSeconds) {
  Super::Tick(DeltaSeconds);

  // 1) 공격 시작 정렬(카메라 방향으로 RInterpTo)
  UpdateAttackAlign(DeltaSeconds);

  // [Changed] TIP 로직 삭제/비활성화 -> 항상 스트레이프 업데이트 호출
  UpdateStrafeYawFollowBySpeed();

  UpdateDirectionalSpeed();
  if (GuardComponent) {
    GuardComponent->UpdateGuardDirAndSpeed();
  }

  // ── [New] 머리 위 닉네임 거리별 가시성 제어 (멀리 있는 타인 닉네임 숨김: 18m) ──
  if (OverheadNameWidget)
  {
      if (IsLocallyControlled())
      {
          if (OverheadNameWidget->IsVisible() != bShowMyOverheadName)
          {
              OverheadNameWidget->SetVisibility(bShowMyOverheadName);
          }
      }
      else
      {
          bool bShouldShow = false;
          if (APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0))
          {
              const float DistSq = FVector::DistSquared(CamMgr->GetCameraLocation(), GetActorLocation());
              constexpr float MaxNameVisibleDistSq = 1800.f * 1800.f; // 약 18미터 이내에서만 표시
              bShouldShow = (DistSq <= MaxNameVisibleDistSq);
          }
          if (OverheadNameWidget->IsVisible() != bShouldShow)
          {
              OverheadNameWidget->SetVisibility(bShouldShow);
          }
      }
  }

  // [New] 타겟 프레임 업데이트
  if (IsLocallyControlled()) // [Fix] HasAuthority() 체크 삭제
                             // (싱글플레이/리슨서버 호스트 위해)
  {
    // ── [New] 크로스헤어 타겟 조준 및 아웃라인 업데이트 ──
    AActor* AimedActor = nullptr;
    
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC)
    {
        FVector CameraLoc;
        FRotator CameraRot;
        PC->GetPlayerViewPoint(CameraLoc, CameraRot);
        
        FVector Start = CameraLoc;
        FVector End = Start + (CameraRot.Vector() * 2500.f); // 최대 사거리 25m
        
        FHitResult HitResult;
        FCollisionQueryParams TraceParams;
        TraceParams.AddIgnoredActor(this);
        
        FCollisionShape SphereShape = FCollisionShape::MakeSphere(50.f);
        // 1차: 살아있는 캐릭터 (Pawn 채널) 탐색
        bool bHit = GetWorld()->SweepSingleByChannel(HitResult, Start, End, FQuat::Identity, ECC_Pawn, SphereShape, TraceParams);

        // 2차: 살아있는 대상을 못 찾았으면 상호작용 대상 (몬스터 사체, 아이템 등) 탐색
        if (!bHit || !HitResult.GetActor())
        {
            static const ECollisionChannel InteractTraceChannel = ECollisionChannel::ECC_GameTraceChannel1;
            bHit = GetWorld()->SweepSingleByChannel(HitResult, Start, End, FQuat::Identity, InteractTraceChannel, SphereShape, TraceParams);
        }

        if (bHit && HitResult.GetActor())
        {
            AActor* HitActor = HitResult.GetActor();
            AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(HitActor);
            
            // 1) 살아있는 적 캐릭터인 경우
            if (Enemy && !Enemy->IsDead())
            {
                AimedActor = HitActor;
            }
            // 2) 상호작용 인터페이스 구현 오브젝트 체크 (몬스터 사체, NPC 등)
            else if (HitActor->GetClass()->ImplementsInterface(UNonInteractableInterface::StaticClass()))
            {
                bool bAlreadyLooted = false;
                if (ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetController()))
                {
                    bAlreadyLooted = MyPC->HasLootedCorpse(HitActor);
                }
                if (!bAlreadyLooted)
                {
                    AimedActor = HitActor;
                }
            }
            // 3) 일반 캐릭터 체크 (NPC, 플레이어 등)
            else if (ACharacter* Char = Cast<ACharacter>(HitActor))
            {
                AimedActor = HitActor;
            }
        }
    }
    
    // 타겟 아웃라인 활성화/비활성화 처리
    if (AimedActor != FocusedActor)
    {
        // 1. 이전 대상의 하이라이트 해제
        if (FocusedActor)
        {
            AEnemyCharacter* TargetEnemy = Cast<AEnemyCharacter>(FocusedActor);
            if (TargetEnemy && !TargetEnemy->IsDead())
            {
                if (USkeletalMeshComponent* MeshComp = TargetEnemy->GetMesh())
                {
                    MeshComp->SetRenderCustomDepth(false);
                }
            }
            else if (FocusedActor->GetClass()->ImplementsInterface(UNonInteractableInterface::StaticClass()))
            {
                INonInteractableInterface::Execute_SetInteractHighlight(FocusedActor, false);
            }
            else if (ACharacter* PrevChar = Cast<ACharacter>(FocusedActor))
            {
                if (USkeletalMeshComponent* MeshComp = PrevChar->GetMesh())
                {
                    MeshComp->SetRenderCustomDepth(false);
                }
            }
        }
        
        // 2. 신규 대상의 하이라이트 설정
        FocusedActor = AimedActor;
        if (FocusedActor)
        {
            AEnemyCharacter* TargetEnemy = Cast<AEnemyCharacter>(FocusedActor);
            if (TargetEnemy && !TargetEnemy->IsDead())
            {
                // 살아있는 적은 외곽선(하이라이트)을 켜지 않고, 화면 상단 타겟 UI(HP바)만 표시합니다.
            }
            else if (FocusedActor->GetClass()->ImplementsInterface(UNonInteractableInterface::StaticClass()))
            {
                INonInteractableInterface::Execute_SetInteractHighlight(FocusedActor, true);
            }
            else if (ACharacter* NewChar = Cast<ACharacter>(FocusedActor))
            {
                if (USkeletalMeshComponent* MeshComp = NewChar->GetMesh())
                {
                    MeshComp->SetRenderCustomDepth(true);
                    MeshComp->SetCustomDepthStencilValue(1); 
                }
            }
        }
    }

    // 전투 중 타겟(CurrentTarget) 정리/선택: 전투 중에는 유지, 전투가 끝나거나 대상이 죽으면 해제
    UpdateCombatTargetForHUD();

    // 크로스헤어로 조준한 적을 우선 표시하고, 조준한 적이 없다면 전투 타겟(CurrentTarget)을 표시
    AEnemyCharacter* TargetToDisplay = Cast<AEnemyCharacter>(FocusedActor);
    if (!TargetToDisplay || TargetToDisplay->IsDead())
    {
        TargetToDisplay = CurrentTarget;
    }

    if (TargetToDisplay && !TargetToDisplay->IsDead()) {
      // 전투 타겟은 거리와 관계없이 전투 중 계속 표시 (조준 대상은 트레이스 사거리 25m 이내만 잡힘)
      const float Dist = GetDistanceTo(TargetToDisplay);
      if (UIManagerComponent) {
        FString EnemyName = TargetToDisplay->GetEnemyName();

        float CurHP = 0.f;
        float MaxHP = 100.f;

        if (UNonAttributeSet *EnemyAS = TargetToDisplay->GetAttributeSet()) {
          CurHP = EnemyAS->GetHP();
          MaxHP = EnemyAS->GetMaxHP();
        }

        UIManagerComponent->UpdateTargetHUD(TargetToDisplay, EnemyName, CurHP, MaxHP, Dist);
      }
    } else {
      // 타겟 없음 -> 숨김
      if (UIManagerComponent) {
        UIManagerComponent->UpdateTargetHUD(nullptr, TEXT(""), 0, 0, 0);
      }
    }
  }
}

void ANonCharacterBase::NotifyEnemyAggroChanged(AEnemyCharacter *Enemy, bool bTargetingMe) {
  if (!Enemy || !IsLocallyControlled())
    return;

  if (bTargetingMe) {
    AggroingEnemies.AddUnique(Enemy);
    // 표시 중인 전투 타겟이 없으면 나를 노리기 시작한 적을 바로 표시
    if (!CurrentTarget || CurrentTarget->IsDead()) {
      SetCombatTarget(Enemy);
    }
  } else {
    AggroingEnemies.Remove(Enemy);
  }
}

void ANonCharacterBase::UpdateCombatTargetForHUD() {
  AggroingEnemies.RemoveAll([](const TWeakObjectPtr<AEnemyCharacter> &E) {
    return !E.IsValid() || E->IsDead();
  });

  const bool bInCombatNow = IsInCombat();

  // 1) 대상이 죽었거나, 전투가 끝났으면 해제
  //    (적중 직후 State.InCombat 태그가 아직 복제되지 않았을 수 있어 설정 후 1초는 유지)
  if (CurrentTarget) {
    const bool bGrace = (GetWorld()->GetTimeSeconds() - LastTargetSetTime) < 1.0f;
    if (CurrentTarget->IsDead() || (!bInCombatNow && !bGrace)) {
      SetCombatTarget(nullptr);
    }
  }

  // 2) 전투 중인데 표시할 대상이 없으면, 나를 노리는 적 중 가장 가까운 적을 표시
  if (!CurrentTarget && bInCombatNow && AggroingEnemies.Num() > 0) {
    AEnemyCharacter *Best = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();
    for (const TWeakObjectPtr<AEnemyCharacter> &E : AggroingEnemies) {
      const float DSq = FVector::DistSquared(GetActorLocation(), E->GetActorLocation());
      if (DSq < BestDistSq) {
        BestDistSq = DSq;
        Best = E.Get();
      }
    }
    SetCombatTarget(Best);
  }
}

void ANonCharacterBase::SetCombatTarget(AEnemyCharacter *NewTarget) {
  if (CurrentTarget == NewTarget)
    return;

  // [New] 타겟 고정 (Sticky): 너무 빠르게 타겟이 바뀌는 것 방지 (0.5초 쿨타임)
  // 단, NewTarget이 nullptr(타겟 해제)인 경우는 즉시 처리
  if (NewTarget != nullptr && CurrentTarget != nullptr) {
    const float TimeSinceLastSet =
        GetWorld()->GetTimeSeconds() - LastTargetSetTime;
    if (TimeSinceLastSet < 0.5f)
      return;
  }

  CurrentTarget = NewTarget;
  LastTargetSetTime = GetWorld()->GetTimeSeconds(); // 시간 기록

  // 타겟 변경 즉시 UI 갱신 시도
  if (IsLocallyControlled()) {
    if (!CurrentTarget && UIManagerComponent) {
      UIManagerComponent->UpdateTargetHUD(nullptr, TEXT(""), 0, 0, 0);
    }
  }
}

void ANonCharacterBase::UpdateDirectionalSpeed() {
  UCharacterMovementComponent *Move = GetCharacterMovement();
  if (!Move)
    return;

  // 1) 내가 컨트롤하는 캐릭터가 아니면(서버의 내 캐릭터 사본 or 다른
  // 클라이언트의 사본),
  //    입력 벡터가 없으므로 로직을 수행하지 않고 리플리케이션된
  //    변수(bIsBackpedaling)에 의존해야 함. 따라서 로컬 컨트롤러인 경우에만
  //    '판단'을 수행.
  if (!IsLocallyControlled()) {
    // 리모트 프록시는 OnRep_IsBackpedaling에서 속도가 처리됨.
    // 다만 Server에서는 OnRep이 안 불리므로, ServerSetBackpedaling에서 처리.
    return;
  }

  auto RestoreDefault = [&]() {
    if (WalkSpeed_Default > 0.f) {
      // 이미 기본값이면 스킵 (불필요한 RPC/Setter 방지)
      if (FMath::IsNearlyEqual(Move->MaxWalkSpeed, WalkSpeed_Default))
        return;

      Move->MaxWalkSpeed = WalkSpeed_Default;
    }
  };

  // Strafe 모드가 아니거나 입력이 없으면 해제
  if (!bStrafeMode) {
    if (bIsBackpedaling)
      ServerSetBackpedaling(false); // 상태 해제 요청
    RestoreDefault();
    return;
  }

  const FVector2D Input2D =
      FVector2D(GetLastMovementInputVector().X, GetLastMovementInputVector().Y);
  const bool bHasInput = Input2D.SizeSquared() > KINDA_SMALL_NUMBER;

  if (!bHasInput || Move->IsFalling()) {
    if (bIsBackpedaling)
      ServerSetBackpedaling(false);
    RestoreDefault();
    return;
  }

  const FVector ControlFwd =
      FRotationMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f))
          .GetUnitAxis(EAxis::X);
  const FVector Input3D = FVector(Input2D.X, Input2D.Y, 0.f).GetSafeNormal();
  const float Dot = FVector::DotProduct(Input3D, ControlFwd);

  const float BackDotThreshold =
      FMath::Cos(FMath::DegreesToRadians(BackpedalDirThresholdDeg));
  const bool bNewBackpedal = (Dot <= BackDotThreshold);

  // 상태 변경 감지
  if (bNewBackpedal != bIsBackpedaling) {
    ServerSetBackpedaling(bNewBackpedal);
  }

  // [Client Prediction] 즉시 적용
  if (bNewBackpedal) {
    Move->MaxWalkSpeed = WalkSpeed_Backpedal;

    // 현재 속도가 제한보다 빠르면 즉시 감속 (관성 제거)
    const float Speed2D = Move->Velocity.Size2D();
    if (Speed2D > WalkSpeed_Backpedal + 1.f) {
      const FVector NewVel =
          FVector(Move->Velocity.X, Move->Velocity.Y, 0.f).GetSafeNormal() *
          WalkSpeed_Backpedal;
      Move->Velocity = FVector(NewVel.X, NewVel.Y, Move->Velocity.Z);
    }
  } else {
    RestoreDefault();
  }
}

void ANonCharacterBase::ServerSetBackpedaling_Implementation(
    bool bNewBackpedaling) {
  bIsBackpedaling = bNewBackpedaling;

  // 서버에서도 속도 적용 (Physics Authority)
  if (UCharacterMovementComponent *Move = GetCharacterMovement()) {
    if (bIsBackpedaling) {
      Move->MaxWalkSpeed = WalkSpeed_Backpedal;
    } else {
      if (WalkSpeed_Default > 0.f)
        Move->MaxWalkSpeed = WalkSpeed_Default;
    }
  }

  // OnRep은 서버에서 호출되지 않으므로, 연결된 다른 클라이언트에게는 자동
  // 복제됨.
}

void ANonCharacterBase::OnRep_IsBackpedaling() {
  // 리모트 클라이언트(다른 플레이어)가 보는 나의 속도 적용
  if (UCharacterMovementComponent *Move = GetCharacterMovement()) {
    if (bIsBackpedaling) {
      Move->MaxWalkSpeed = WalkSpeed_Backpedal;
    } else {
      if (WalkSpeed_Default > 0.f)
        Move->MaxWalkSpeed = WalkSpeed_Default;
    }
  }
}

// 이동 입력 여부를 서버/클라이언트 양쪽에서 같게 판단한다.
// GetLastMovementInputVector()는 입력을 받은 로컬 클라이언트에만 값이 있고 서버에서는 항상 0이므로,
// 0이면 CharacterMovement의 현재 가속도(클라이언트 이동 정보로 서버에도 전달됨)로 판단한다.
// (서버가 "이동 입력 없음"으로 회전을 멈추면 서버/클라 방향이 어긋나 피격 판정이 틀리고,
//  루트 모션 몽타주 재생 시 클라가 서버 방향으로 보정되어 캐릭터가 휙 도는 현상이 생김)
static bool HasMoveInputNetSafe(const ACharacter* Char, const UCharacterMovementComponent* Move) {
  if (!Char) return false;
  if (Char->GetLastMovementInputVector().SizeSquared() > KINDA_SMALL_NUMBER) return true;
  return Move && Move->GetCurrentAcceleration().SizeSquared2D() > KINDA_SMALL_NUMBER;
}

void ANonCharacterBase::UpdateStrafeYawFollowBySpeed() {
  UCharacterMovementComponent *Move = GetCharacterMovement();
  if (!Move)
    return;

  // 1. 태그 확인 (스킬, 공격, 회피 등)
  bool bHasLockTag = false;
  if (AbilitySystemComponent) {
    const FGameplayTag DodgeTag =
        NonTags::State_Dodge.GetTag();
    const FGameplayTag AttackTag =
        NonTags::State_Attack.GetTag();
    const FGameplayTag AttackingTag =
        NonTags::State_Attacking.GetTag();
    const FGameplayTag ComboTag =
        NonTags::Ability_Combo.GetTag();
    const FGameplayTag Combo1Tag =
        NonTags::Ability_Combo1.GetTag();
    const FGameplayTag Combo2Tag =
        NonTags::Ability_Combo2.GetTag();
    const FGameplayTag Combo3Tag =
        NonTags::Ability_Combo3.GetTag();
    const FGameplayTag SkillTag =
        NonTags::State_Skill.GetTag();
    const FGameplayTag HitReactTag =
        NonTags::State_HitReacting.GetTag();
    const FGameplayTag StunTag =
        NonTags::State_Stunned.GetTag();
    const FGameplayTag DeadTag =
        NonTags::State_Dead.GetTag();
    const FGameplayTag ToggleWeaponRootTag =
        NonTags::State_ToggleWeapon_Root.GetTag();

    const bool bDodge =
        AbilitySystemComponent->HasMatchingGameplayTag(DodgeTag);
    const bool bAttack =
        AbilitySystemComponent->HasMatchingGameplayTag(AttackTag) ||
        AbilitySystemComponent->HasMatchingGameplayTag(AttackingTag);
    const bool bCombo =
        AbilitySystemComponent->HasMatchingGameplayTag(ComboTag) ||
        AbilitySystemComponent->HasMatchingGameplayTag(Combo1Tag) ||
        AbilitySystemComponent->HasMatchingGameplayTag(Combo2Tag) ||
        AbilitySystemComponent->HasMatchingGameplayTag(Combo3Tag);
    const bool bSkill =
        AbilitySystemComponent->HasMatchingGameplayTag(SkillTag);

    // 🔥 풀바디 스킬만 회전을 잠그고, 상체 전용 스킬(버프 스킬 등)은 이동 및 회전을 잠그지 않음!
    const bool bFullBodySkill = bSkill && bForceFullBody;
    const bool bUpperBodySkill = bSkill && !bForceFullBody;
    
    // [Fix] 피격, 스턴, 사망 시에도 회전 잠금 처리
    const bool bStunned = 
        AbilitySystemComponent->HasMatchingGameplayTag(HitReactTag) ||
        AbilitySystemComponent->HasMatchingGameplayTag(StunTag) ||
        AbilitySystemComponent->HasMatchingGameplayTag(DeadTag);

    const bool bToggleWeapon =
        AbilitySystemComponent->HasMatchingGameplayTag(ToggleWeaponRootTag);

    bHasLockTag = (bDodge || bAttack || bCombo || bFullBodySkill || bStunned || bToggleWeapon);
  }


  // 2. [1순위] 어빌리티 사용 중(공격/회피/피격 등): 가드 상태보다 우선하여 무조건 회전 잠금
  if (bHasLockTag) {
    if (!bRotationLockedByAbility) {
      bRotationLockedByAbility = true;
      bAlignYawAfterRotationLock = false;
    }

    bFollowCameraYawNow = false;
    bUseControllerRotationYaw = false;
    Move->bUseControllerDesiredRotation = false;
    Move->bOrientRotationToMovement = false;
    Move->RotationRate = FRotator(0.f, 0.f, 0.f);
    return;
  }

  // 3. [2순위] 가드 중: 이동 시 스트레이프, 완전 정지 시 제자리 턴(Step Turn) 또는 화면 고정 수행
  if (IsGuarding()) {
    // 🛡️ [화면 고정 / 스트레이프 락 모드 (다크소울 / 엘든링 표준)]:
    // 턴 몽타주를 완전히 끄고, 마우스를 돌리는 대로 초당 720도의 날렵하고 부드러운 속도로 카메라를 1:1 주시!
    // 발 꼬임, 몽타주 끊김, 상체 휘청임 등의 모든 골치 아픈 문제가 100% 완전 소멸!
    if (!bEnableGuardTurnMontage) {
      const bool bHasMoveInput = HasMoveInputNetSafe(this, Move);

      // 🗡️ [몬스터헌터 대검/랜스 스타일]: 가드 중 정지 시 회전 완전 차단 & WASD 이동할 때만 회전!
      if (bGuardRotateOnlyOnMove && !bHasMoveInput) {
        bUseControllerRotationYaw = false;
        Move->bUseControllerDesiredRotation = false;
        Move->bOrientRotationToMovement = false;
        Move->RotationRate = FRotator(0.f, 0.f, 0.f); // 정지 중 회전 완전 차단 (카메라만 둘러봄)
        bFollowCameraYawNow = false;
      } else {
        // [다크소울 / 엘든링 스타일]: 마우스를 돌리는 대로 초당 720도의 날렵한 속도로 카메라 주시
        bUseControllerRotationYaw = false;
        Move->bUseControllerDesiredRotation = true;
        Move->bOrientRotationToMovement = false;
        Move->RotationRate = FRotator(0.f, 720.f, 0.f);
        bFollowCameraYawNow = true;
      }

      if (bIsGuardTurning) {
        bIsGuardTurning = false;
        if (ActiveGuardTurnMontage) {
          if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr) {
            AnimInst->Montage_Stop(0.15f, ActiveGuardTurnMontage);
          }
          ActiveGuardTurnMontage = nullptr;
        }
        if (MotionWarpingComp) {
          MotionWarpingComp->RemoveWarpTarget(GuardTurnWarpTargetName);
        }
      }

      bRotationLockedByAbility = false;
      bAlignYawAfterRotationLock = false;
      return;
    }

    const bool bHasMoveInput = HasMoveInputNetSafe(this, Move);
    const float CurrentSpeed = Move->Velocity.Size2D();
    // 🌟 루트 모션 회전 중 발생하는 자체 속도로 인해 턴이 취소되는 것을 방지하기 위해 !bIsGuardTurning 조건 적용
    const bool bIsMoving = bHasMoveInput || (!bIsGuardTurning && CurrentSpeed > 30.f);

    if (bIsMoving) {
      // [이동 또는 감속 중]: 가드 상태로 걷거나 멈추는 중에는 카메라 방향을 자연스럽게 바라보며 스트레이프
      // ★ 1프레임 하드 스냅 차단! bUseControllerRotationYaw 대신 bUseControllerDesiredRotation(720도/초)으로 부드럽게 카메라 추종
      bUseControllerRotationYaw = false;
      Move->bUseControllerDesiredRotation = true;
      Move->bOrientRotationToMovement = false;
      Move->RotationRate = FRotator(0.f, 720.f, 0.f);
      bFollowCameraYawNow = true;

      // 실제 플레이어 이동 입력(WASD)이 들어왔을 때: 턴 몽타주를 0.25초 부드럽게 블렌드아웃하고 즉시 걷기로 전환
      if (bHasMoveInput && bIsGuardTurning) {
        bIsGuardTurning = false;
        if (ActiveGuardTurnMontage) {
          if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr) {
            AnimInst->Montage_Stop(0.15f, ActiveGuardTurnMontage); // 빠른 블렌드아웃(0.15초)으로 루트모션 구속 즉시 해제 및 걷기 가속 즉시 시작
          }
          ActiveGuardTurnMontage = nullptr;
        }
        if (MotionWarpingComp) {
          MotionWarpingComp->RemoveWarpTarget(GuardTurnWarpTargetName); // 모션 워핑 회전 간섭 즉시 해제
        }
        GuardTurnPendingTime = 0.f;
        GuardTurnCooldown = 0.2f;
      }
    } else {
      // [완전 정지 상태]: 팽이 회전 차단! 몸체는 고정하고 마우스 회전 시 턴 몽타주(Step Turn)로 발을 디디며 회전
      bUseControllerRotationYaw = false;
      Move->bUseControllerDesiredRotation = false;
      Move->bOrientRotationToMovement = false;
      Move->RotationRate = FRotator(0.f, 0.f, 0.f);
      bFollowCameraYawNow = false;

      const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
      UpdateGuardTurnInPlace(DeltaSeconds);
    }

    // 다른 잠금 상태 강제 해제
    bRotationLockedByAbility = false;
    bAlignYawAfterRotationLock = false;
    return;
  }

  // 4. 어빌리티 종료 직후: 정면 정렬 (Alignment)
  if (bRotationLockedByAbility) {
    bRotationLockedByAbility = false;
    // [Change] 회피/공격 후 카메라 방향으로 자동 정렬되는 기능 끄기
    // bAlignYawAfterRotationLock = true;
    bAlignYawAfterRotationLock = false; // 수동 조작 시 자연스럽게 돌아가도록
  }

  if (bAlignYawAfterRotationLock) {
    const float DeltaSeconds =
        GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
    const FRotator Current = GetActorRotation();
    FRotator Target = GetControlRotation();
    Target.Pitch = Current.Pitch;
    Target.Roll = Current.Roll;

    const FRotator NewRot = FMath::RInterpTo(Current, Target, DeltaSeconds,
                                             RotationAlignInterpSpeed);
    SetActorRotation(NewRot);

    const float DeltaYaw =
        FMath::Abs(FMath::FindDeltaAngleDegrees(NewRot.Yaw, Target.Yaw));
    if (DeltaYaw < 1.f) {
      bAlignYawAfterRotationLock = false; // 정렬 완료
    } else {
      // 정렬 중에는 조작 회전 차단
      bFollowCameraYawNow = false;
      bUseControllerRotationYaw = false;
      Move->bUseControllerDesiredRotation = false;
      Move->bOrientRotationToMovement = false;
      return;
    }
  }

  // 🔥 상체 스킬(버프 스킬 등) 활성화 여부
  const bool bUpperBodySkillActive = AbilitySystemComponent &&
      AbilitySystemComponent->HasMatchingGameplayTag(NonTags::State_Skill.GetTag()) &&
      !bForceFullBody;

  // 5. [Base Locomotion] 전투(Armed) vs 비전투(Unarmed)
  if (bStrafeMode) // Armed
  {
    // [New] Input 기반 스트레이프 Check
    // 입력이 있으면 -> 즉시 스트레이프(카메라 정렬)
    // 입력이 없으면 -> 정렬 끔(Idle Free Look)
    const FVector InputVec = GetLastMovementInputVector();
    bool bHasInput = (InputVec.SizeSquared() > KINDA_SMALL_NUMBER);

    // [Fix] 서버에서는 InputVector가 0일 수 있으므로(타이밍 이슈), 속도가
    // 있으면 입력 중으로 간주
    if (HasAuthority() && !bHasInput) {
      bHasInput = (GetVelocity().SizeSquared2D() > 10.f);
    }

    // 🔥 상체 스킬(버프 스킬 등) 시전 중이거나 이동 입력이 있을 때는 마우스(카메라) 방향 추종 회전!
    if (bHasInput || bUpperBodySkillActive) {
      // [Fix] 가드 해제 시 bUseControllerRotationYaw가 true 상태로 계속 고착되는 현상 방지
      bUseControllerRotationYaw = false;
      Move->bUseControllerDesiredRotation = true;
      Move->bOrientRotationToMovement = false;
      Move->RotationRate = FRotator(0.f, 720.f, 0.f); // 즉각적이고 부드러운 마우스 추종 회전!
      bFollowCameraYawNow = true;
    } else {
      // 입력 없으면 카메라만 돌릴 수 있게 회전 잠금 해제
      // [Fix] 잠금 직후 복구 시점을 위해 DesiredRotation 상태도 체크
      if (bFollowCameraYawNow || Move->bUseControllerDesiredRotation) {
        bFollowCameraYawNow = false;
        bUseControllerRotationYaw = false;
        Move->bUseControllerDesiredRotation = false;
        Move->bOrientRotationToMovement = false;
      }
    }
  } else // Unarmed
  {
    if (bUpperBodySkillActive)
    {
      // 비전투 상태에서 버프 시전 시에도 마우스(카메라) 방향 추종
      bFollowCameraYawNow = true;
      bUseControllerRotationYaw = false;
      Move->bUseControllerDesiredRotation = true;
      Move->bOrientRotationToMovement = false;
      Move->RotationRate = FRotator(0.f, 720.f, 0.f);
    }
    else
    {
      // 무조건 이동 방향 바라보기 (Free Look)
      // [Fix] bFollowCameraYawNow 뿐만 아니라 실제 설정이 틀어져 있을 때도 맞춰주어 부활 후 회전 잠금 해결
      if (bFollowCameraYawNow || !Move->bOrientRotationToMovement) {
        bFollowCameraYawNow = false;
        bUseControllerRotationYaw = false;
        Move->bUseControllerDesiredRotation = false;
        Move->bOrientRotationToMovement = true;
        Move->RotationRate = FRotator(0.f, 540.f, 0.f);
      }
    }
  }
}

void ANonCharacterBase::LookInput(const FInputActionValue &Value) {
  // 조준 데칼 조작 중이거나 상점 UI가 띄워졌을 때만 카메라 회전을 잠급니다.
  if (bLookInputBlocked || (UIManagerComponent && UIManagerComponent->IsMerchantShopOpen())) return;

  FVector2D LookAxis = Value.Get<FVector2D>();
  AddControllerYawInput(LookAxis.X);
  AddControllerPitchInput(LookAxis.Y);
}

void ANonCharacterBase::HandleAttackInput(const FInputActionValue &Value) {
  // 설계서 3.7 확정: 납도 상태에서 평타 입력 시 발도 모션만 재생 (입력 버퍼로 공격을 이어서 실행하지 않음)
  if (TryAutoDrawWeapon()) {
    return;
  }

  if (IsComboWindowOpen()) {
    BufferComboInput();
  } else {
    TryActivateCombo();
  }
}

void ANonCharacterBase::PossessedBy(AController *NewController) {
  Super::PossessedBy(NewController);

  if (APlayerController *PC = Cast<APlayerController>(NewController)) {
    if (UEnhancedInputLocalPlayerSubsystem *Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
                PC->GetLocalPlayer())) {
      if (DefaultMappingContext) {
        Subsystem->AddMappingContext(DefaultMappingContext, 0);
      }
    }
  }

  if (AbilitySystemComponent) {
    AbilitySystemComponent->InitAbilityActorInfo(this, this);
    BindASCDelegates();

    if (HasAuthority()) {
      InitializeAttributes();
      GiveStartupAbilities();
    }
  }
}

void ANonCharacterBase::SetupPlayerInputComponent(
    UInputComponent *PlayerInputComponent) {
  Super::SetupPlayerInputComponent(PlayerInputComponent);

  // [Refactor] 모든 입력 바인딩을 NonPlayerController로 이관했습니다.
  // 독립형 캐릭터(AI가 아닌 Player가 빙의하는데 Controller 없는 경우)가
  // 필요하면 복구하세요.
}

UAbilitySystemComponent *ANonCharacterBase::GetAbilitySystemComponent() const {
  return AbilitySystemComponent;
}

void ANonCharacterBase::InitializeAttributes() {
  if (!AbilitySystemComponent) {
    return;
  }

  if (!HasAuthority())
    return;

  // [Fix] 이미 초기화되었으면 스킵 (BeginPlay와 PossessedBy 중복 호출 방지)
  if (bAttributesInitialized) {

    return;
  }
  bAttributesInitialized = true;

  // 1) 기본 스탯 초기화 (GE_StatInit)
  if (DefaultAttributeEffect) {
    FGameplayEffectContextHandle EffectContext =
        AbilitySystemComponent->MakeEffectContext();
    EffectContext.AddSourceObject(this);

    FGameplayEffectSpecHandle NewHandle =
        AbilitySystemComponent->MakeOutgoingSpec(DefaultAttributeEffect, 1.0f,
                                                 EffectContext);

    if (NewHandle.IsValid()) {
      AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(
          *NewHandle.Data.Get(), AbilitySystemComponent);
    }
  }

  // 2) 스태미나 리젠 GE 적용 (GE_SPRegen 같은 것)
  if (StaminaRegenEffectClass) {
    FGameplayEffectContextHandle EffectContext =
        AbilitySystemComponent->MakeEffectContext();
    EffectContext.AddSourceObject(this);

    FGameplayEffectSpecHandle RegenHandle =
        AbilitySystemComponent->MakeOutgoingSpec(StaminaRegenEffectClass, 1.0f,
                                                 EffectContext);

    if (RegenHandle.IsValid()) {
      AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(
          *RegenHandle.Data.Get(), AbilitySystemComponent);
    }
  }

  // 3) 마나(MP) 리젠 GE 적용
  if (ManaRegenEffectClass) {
    FGameplayEffectContextHandle EffectContext =
        AbilitySystemComponent->MakeEffectContext();
    EffectContext.AddSourceObject(this);

    FGameplayEffectSpecHandle RegenHandle =
        AbilitySystemComponent->MakeOutgoingSpec(ManaRegenEffectClass, 1.0f,
                                                 EffectContext);

    if (RegenHandle.IsValid()) {
      AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(
          *RegenHandle.Data.Get(), AbilitySystemComponent);
    }
  }

  // 4) 체력(HP) 리젠 GE 적용
  if (HealthRegenEffectClass) {
    FGameplayEffectContextHandle EffectContext =
        AbilitySystemComponent->MakeEffectContext();
    EffectContext.AddSourceObject(this);

    FGameplayEffectSpecHandle RegenHandle =
        AbilitySystemComponent->MakeOutgoingSpec(HealthRegenEffectClass, 1.0f,
                                                 EffectContext);

    if (RegenHandle.IsValid()) {
      AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(
          *RegenHandle.Data.Get(), AbilitySystemComponent);
    }
  }
}

void ANonCharacterBase::GiveStartupAbilities() {
  if (!HasAuthority() || !AbilitySystemComponent) {
    return;
  }

  // 1) 공통 GA 먼저 부여 (모든 직업 공통으로 쓰는 것들)
  for (const TSubclassOf<UGameplayAbility> &AbilityClass : DefaultAbilities) {
    if (AbilityClass) {
      AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(
          AbilityClass, 1,
          static_cast<int32>(
              AbilitySystemComponent->GetActivatableAbilities().Num()),
          this));
    }
  }

  // 1-1) 물약 복용 GA: DefaultAbilities에 따로 넣지 않았으면 기본 클래스로 부여
  //      (BP에서 몽타주 등을 설정한 GA_ConsumePotion 파생 BP를 DefaultAbilities에 넣으면 그쪽이 쓰임)
  {
    bool bHasPotionAbility = false;
    for (const TSubclassOf<UGameplayAbility> &AbilityClass : DefaultAbilities) {
      if (AbilityClass && AbilityClass->IsChildOf(UGA_ConsumePotion::StaticClass())) {
        bHasPotionAbility = true;
        break;
      }
    }
    if (!bHasPotionAbility) {
      AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UGA_ConsumePotion::StaticClass(), 1, INDEX_NONE, this));
    }
  }

  // 2) 직업별 GA 세트 적용
  const EJobClass JobToUse = DefaultJobClass; // 지금은 DefaultJobClass 기준

  if (const FJobAbilitySet *FoundSet = JobAbilitySets.Find(JobToUse)) {
    for (const TSubclassOf<UGameplayAbility> &AbilityClass :
         FoundSet->Abilities) {
      if (AbilityClass) {
        AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(
            AbilityClass, 1,
            static_cast<int32>(
                AbilitySystemComponent->GetActivatableAbilities().Num()),
            this));
      }
    }
  }
}

void ANonCharacterBase::SetComboWindowOpen(bool bOpen) {
  bComboWindowOpen = bOpen;

  if (CurrentComboAbility.IsValid()) {
    CurrentComboAbility->SetComboWindowOpen(bOpen);
  }
}

void ANonCharacterBase::BufferComboInput() {
  if (CurrentComboAbility.IsValid()) {
    CurrentComboAbility->BufferComboInput();
  }
}

bool ANonCharacterBase::IsComboWindowOpen() const { return bComboWindowOpen; }

void ANonCharacterBase::TryActivateCombo() {
  if (!AbilitySystemComponent) {
    return;
  }

  // 이미 콤보 어빌리티(Ability.Combo)가 돌고 있으면 새로 시작하지 않음
  const FGameplayTag ActiveComboTag =
      NonTags::Ability_Combo.GetTag();
  if (AbilitySystemComponent->HasMatchingGameplayTag(ActiveComboTag)) {
    return;
  }

  // Ability.Combo1 태그를 가진 GA를 찾아서 실행
  const FGameplayTag Combo1Tag =
      NonTags::Ability_Combo1.GetTag();

  FGameplayTagContainer TagContainer;
  TagContainer.AddTag(Combo1Tag);

  const bool bSuccess =
      AbilitySystemComponent->TryActivateAbilitiesByTag(TagContainer);
}

// 등록/해제
void ANonCharacterBase::RegisterCurrentComboAbility(UGA_ComboBase *InAbility) {
  CurrentComboAbility = InAbility;
}
void ANonCharacterBase::UnregisterCurrentComboAbility(
    UGA_ComboBase *InAbility) {
  if (CurrentComboAbility.Get() == InAbility) {
    CurrentComboAbility = nullptr;
  }
}

void ANonCharacterBase::ClearCurrentSwingHitActors() {
  CurrentSwingHitActors.Reset();
}

bool ANonCharacterBase::HasHitActorInCurrentSwing(AActor* Target) const {
  if (!Target) return false;
  return CurrentSwingHitActors.Contains(Target);
}

void ANonCharacterBase::AddHitActorInCurrentSwing(AActor* Target) {
  if (Target) {
    CurrentSwingHitActors.Add(Target);
  }
}

void ANonCharacterBase::LevelUp() {

  if (!LevelDataTable || !AttributeSet || !AbilitySystemComponent)
    return;

  // 레벨업은 서버에서만 처리되게 하는 게 안전
  if (!HasAuthority())
    return;

  int32 CurrentLevel = AttributeSet->GetLevel();
  int32 NewLevel = CurrentLevel + 1;

  FLevelData *NewData = LevelDataTable->FindRow<FLevelData>(
      *FString::FromInt(NewLevel), TEXT(""));
  if (!NewData) {
    return;
  }

  FGameplayEffectContextHandle Context;
  FGameplayEffectSpecHandle Spec =
      AbilitySystemComponent->MakeOutgoingSpec(GE_LevelUp, 1.f, Context);
  if (Spec.IsValid()) {
    Spec.Data->SetSetByCallerMagnitude(
        NonTags::Data_MaxHP.GetTag(), NewData->MaxHP);
    Spec.Data->SetSetByCallerMagnitude(
        NonTags::Data_MaxMP.GetTag(), NewData->MaxMP);
    Spec.Data->SetSetByCallerMagnitude(
        NonTags::Data_ExpToNextLevel.GetTag(),
        NewData->ExpToNextLevel);

    AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

    AbilitySystemComponent->SetNumericAttributeBase(
        AttributeSet->GetLevelAttribute(), NewLevel);

    // 1. Max 수치 먼저 갱신
    AbilitySystemComponent->SetNumericAttributeBase(
        AttributeSet->GetMaxHPAttribute(), NewData->MaxHP);
    AbilitySystemComponent->SetNumericAttributeBase(
        AttributeSet->GetMaxMPAttribute(), NewData->MaxMP);

    // 2. 현재 수치 갱신 (Full 회복)
    AbilitySystemComponent->SetNumericAttributeBase(
        AttributeSet->GetHPAttribute(), NewData->MaxHP);
    AbilitySystemComponent->SetNumericAttributeBase(
        AttributeSet->GetMPAttribute(), NewData->MaxMP);

    // ── 스탯/스킬 포인트(어트리뷰트 기반) ──
    // (생략: DataAccess를 통해 포인트 지급 로직 추가 가능)

    // [New] 레벨업 직후 자동 저장 (로비 싱크용)
    if (USaveGameSubsystem *SaveSys =
            GetGameInstance()->GetSubsystem<USaveGameSubsystem>()) {
      SaveSys->SaveGame();
    }
    float CurrentStat = AttributeSet->GetStatPoint();
    float CurrentSkill = AttributeSet->GetSkillPoint();
    AbilitySystemComponent->SetNumericAttributeBase(
        AttributeSet->GetStatPointAttribute(), CurrentStat + 5.f);
    AbilitySystemComponent->SetNumericAttributeBase(
        AttributeSet->GetSkillPointAttribute(), CurrentSkill + 5.f);

    // ── 스킬 매니저 쪽 스킬 포인트(스킬트리용) ──
    if (SkillMgr) {
      // 레벨업당 스킬포인트
      SkillMgr->AddSkillPoints(5);
    }
    // [Fix] HUD 갱신 (로컬 플레이어만)
   
    if (UIManagerComponent && IsLocallyControlled())
    {
      UIManagerComponent->UpdateHP(NewData->MaxHP, NewData->MaxHP);
      UIManagerComponent->UpdateMP(NewData->MaxMP, NewData->MaxMP);
      UIManagerComponent->UpdateEXP(AttributeSet->GetExp(), NewData->ExpToNextLevel);
      UIManagerComponent->UpdateLevel(NewLevel);
    }
  }
}

void ANonCharacterBase::GainExp(float Amount) {
  if (AbilitySystemComponent && AttributeSet) {
    float CurrentExp = AttributeSet->GetExp();
    float MaxExp = AttributeSet->GetExpToNextLevel();
    float NewExp = CurrentExp + Amount;
    for (int i = 0; i < 10; ++i) {
      if (NewExp < MaxExp)
        break;

      NewExp -= MaxExp;
      LevelUp();
      MaxExp = AttributeSet->GetExpToNextLevel();
      if (MaxExp <= 0.f)
        break;
    }

    AbilitySystemComponent->SetNumericAttributeBase(
        AttributeSet->GetExpAttribute(), NewExp);

    if (UIManagerComponent) {
      UIManagerComponent->UpdateEXP(NewExp, AttributeSet->GetExpToNextLevel());
      UIManagerComponent->UpdateLevel(AttributeSet->GetLevel());
    }
  }
}

void ANonCharacterBase::SetLevelAndRefreshStats(int32 NewLevel) {
  if (!AbilitySystemComponent || !AttributeSet || !LevelDataTable)
    return;

  // 1. 레벨 데이터 조회
  FLevelData *NewData = LevelDataTable->FindRow<FLevelData>(
      *FString::FromInt(NewLevel), TEXT(""));
  if (!NewData)
    return;

  // 2. 레벨 및 Max 스탯 강제 설정 (GE 없이 직접 값 설정)
  AbilitySystemComponent->SetNumericAttributeBase(
      AttributeSet->GetLevelAttribute(), static_cast<float>(NewLevel));
  // MaxHP/MaxMP/ExpToNextLevel 등을 데이터 테이블 값으로 갱신 (SetByCaller 대신
  // Base값 직접 수정) 주의: AttributeSet에 해당 Attribute에 대한 Setter나 Base
  // set이 가능해야 함. 여기서는 InitAttribute와 유사하게 BaseValue를 설정.

  // 만약 MaxHP/MaxMP가 Attribute라면 아래와 같이 설정
  // Data.MaxHP -> AttributeSet->GetMaxHPAttribute()

  // 하지만 현재 구조상 LevelUp을 보면 GE_LevelUp을 써서
  // Spec.Data->SetSetByCallerMagnitude로 처리함. 여기서도 동일하게 GE를
  // 적용하되 "레벨업 효과"만 빼고 "스탯 갱신"만 할 수도 있고, 아니면 단순히
  // BaseValue를 직접 박아버리는게 가장 깔끔함 (로드니까).

  // Data.MaxHP, Data.MaxMP, Data.ExpToNextLevel 값이 필요함.
  // NonAttributeSet에 정의된 Attribute 이름 확인 필요. (InitAtribues 참고)
  // 여기서는 LevelUp 로직을 참고하여 GE를 쓰지 않고 값만 맞춤.

  // *중요*: ExpToNextLevel이 갱신되어야 Load시 Exp >= ExpToNextLevel 체크를
  // 통과하지 않음.

  // NOTE: AttributeSet의 정확한 Attribute Getter 필요.
  // 가정: GetMaxHPAttribute(), GetMaxMPAttribute(),
  // GetExpToNextLevelAttribute()

  FGameplayEffectContextHandle Context; // 빈 컨텍스트
  // GE_LevelUp을 사용하여 스탯만 적용 (레벨 증가는 아님, 그냥 해당 레벨의 스탯
  // 적용) NewLevel에 해당하는 스탯을 적용.

  // 단순하게 BaseValue 설정
  AbilitySystemComponent->SetNumericAttributeBase(
      AttributeSet->GetMaxHPAttribute(), NewData->MaxHP);
  AbilitySystemComponent->SetNumericAttributeBase(
      AttributeSet->GetMaxMPAttribute(), NewData->MaxMP);

  // [Fix] 현재 체력/마나도 Max 값으로 맞춰줍니다. (저장된 현재 체력이 없다면
  // 풀피로 시작하는 것이 일반적)
  AbilitySystemComponent->SetNumericAttributeBase(
      AttributeSet->GetHPAttribute(), NewData->MaxHP);
  AbilitySystemComponent->SetNumericAttributeBase(
      AttributeSet->GetMPAttribute(), NewData->MaxMP);

  // ExpToNextLevel은 보통 Attribute가 아니라면? -> 확인해보니 AttributeSet에
  // 있음 (LevelUp 함수 참고) LevelUp 함수:
  // Spec.Data->SetSetByCallerMagnitude(..., NewData->ExpToNextLevel); 즉
  // "Data.ExpToNextLevel" 태그로 매핑됨.

  // 직접 Attribute Base Value 설정이 제일 확실함.
  // AttributeSet->GetExpToNextLevelAttribute() 가 있다면. (보통 매크로로
  // 생성됨)

  // -> 일단 코드를 안전하게 하기 위해 GE_LevelUp을 쓰지 않고 직접 값 주입 시도.
  // 전제: NonAttributeSet에 InitFromMetaDataTable 같은게 있다면 좋겠지만
  // 없으므로 수동 설정.

  // *중요 체크*: NonAttributeSet.h를 못봐서 정확한 Attribute 접근자가 있는지
  // 모름. 하지만 보통 GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName,
  // PropertyName) 매크로를 씀.
  // -> GetExpToNextLevelAttribute() 확인 필요. -> LevelUp함수에서는 안쓰고 GE로
  // 넘김.
  // -> 그래도
  // ASC->SetNumericAttributeBase(AttributeSet->GetExpToNextLevelAttribute(),
  // ...) 는 가능.

  // (확인된 정보: LevelUp 함수에서 SetSetByCallerMagnitude... )
  // 따라서 AttributeSet 헤더 확인 없이도 안전하게 하려면 FindAttribute를
  // 써야하나... 아니면 그냥 가장 일반적인 패턴인 Get...Attribute() 시도.

  AbilitySystemComponent->SetNumericAttributeBase(
      AttributeSet->GetExpToNextLevelAttribute(), NewData->ExpToNextLevel);

  // 현재 HP/MP도 Max로 채워줄지? -> 로드니까 저장된거 쓰겠지만, Max가 늘어나면
  // 비율 조정 혹은 그대로. 여기선 Max만 세팅. 현재 체력은 LoadGame에서 별도로
  // 로드하거나(저장했다면), 아니면 Max로 채움. 보통 초기화니까 Max로 채워주는게
  // 맞을수도 있지만, "로드" 과정의 일부라면 Max만 세팅하고 Cur는 나중에 로드된
  // 값 덮어쓰기.

  // 로드 시점:
  // 1. SetLevelAndRefreshStats(SavedLevel) -> Max 스탯 갱신
  // 2. ASC->SetNumericAttributeBase(Exp, SavedExp)
  // 3. (Optional) HP/MP 복구 (저장 안했다면 Full)

  if (UIManagerComponent) {
    UIManagerComponent->UpdateLevel(NewLevel);
    UIManagerComponent->UpdateHP(NewData->MaxHP, NewData->MaxHP);
    UIManagerComponent->UpdateMP(NewData->MaxMP, NewData->MaxMP);
    UIManagerComponent->UpdateEXP(AttributeSet->GetExp(), NewData->ExpToNextLevel);
  }
}

FName ANonCharacterBase::GetHandSocketForItem(
    const UInventoryItem *Item) const {
  if (!Item)
    return HandSocket1H;
  const bool bTwoHand = Item->IsTwoHandedWeapon();
  return bTwoHand ? HandSocket2H : HandSocket1H;
}

void ANonCharacterBase::RefreshWeaponStance() {
  UEquipmentComponent *Eq = FindComponentByClass<UEquipmentComponent>();
  UInventoryItem *Main =
      Eq ? Eq->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain) : nullptr;
  UInventoryItem *Sub =
      Eq ? Eq->GetEquippedItemBySlot(EEquipmentSlot::WeaponSub) : nullptr;

  // 무장 상태가 아니면 무조건 Unarmed
  if (!bArmed) {
    WeaponStance = EWeaponStance::Unarmed;
    SetStrafeMode(false);
    return;
  }

  // 메인도 없고 서브(방패)도 없으면 Unarmed
  if (!Main && !Sub) {
    WeaponStance = EWeaponStance::Unarmed;
    SetStrafeMode(false);
    return;
  }

  // [New] 스탠스 결정 로직 (SyncEquipped~ 와 동일하게)
  // 1. 기본은 Unarmed
  EWeaponStance NewStance = EWeaponStance::Unarmed;

  if (Main) {
    // 메인 무기가 있을 때만 스탠스 변경
    const bool bIsStaff = (Main->CachedRow.WeaponType == EWeaponType::Staff);
    const bool bIsTwoHand = Main->IsTwoHandedWeapon() || bIsStaff;

    if (bIsStaff)
      NewStance = EWeaponStance::Staff;
    else if (bIsTwoHand)
      NewStance = EWeaponStance::GreatSword;
    else
      NewStance = EWeaponStance::SwordShield; // 그 외(한손검 등)는 SwordShield
  }

  // 최종 적용
  if (WeaponStance != NewStance) {
    WeaponStance = NewStance;
  }

  // 무기를 들었으므로 스트레이프 모드 켜기 (원한다면)
  SetStrafeMode(true);
}

FName ANonCharacterBase::GetHomeFallbackSocketForItem(
    const UInventoryItem *Item) const {
  const bool bTwoHand = (Item && Item->IsTwoHandedWeapon());
  return bTwoHand ? SheathSocket2H : SheathSocket1H;
}

void ANonCharacterBase::GuardPressed() {
  if (TryAutoDrawWeapon())
    return;

  if (CurrentComboAbility.IsValid()) {
    const FGameplayTag Tag_Combo3 =
        NonTags::Ability_Combo3.GetTag();
    if (CurrentComboAbility->GetAssetTags().HasTagExact(Tag_Combo3)) {
      return;
    }
  }

  StartGuard();
}

void ANonCharacterBase::Jump() {
  // 대화 중이거나 상점 UI 조작 상황에서만 점프를 차단합니다.
  if (bIsDialogueCameraActive || (UIManagerComponent && UIManagerComponent->IsMerchantShopOpen())) {
      return;
  }
  Super::Jump();

  if (AbilitySystemComponent) {
    static const FGameplayTag JumpTag =
        NonTags::State_Jump.GetTag();

    AbilitySystemComponent->AddLooseGameplayTag(JumpTag);
  }
}

void ANonCharacterBase::Landed(const FHitResult &Hit) {
  Super::Landed(Hit);

  if (AbilitySystemComponent) {
    static const FGameplayTag JumpTag =
        NonTags::State_Jump.GetTag();

    AbilitySystemComponent->RemoveLooseGameplayTag(JumpTag);
  }
}

void ANonCharacterBase::GuardReleased() { StopGuard(true); }

void ANonCharacterBase::ApplyDamageAt(float Amount, AActor *DamageInstigator,
                                      const FVector &WorldLocation,
                                      FGameplayTag ReactionTag,
                                      bool bRangedHit) {
  // 이미 죽은 상태라면 데미지를 무시합니다.
  if (IsDead()) return;

  // ── [New] 동일 공격자 0.25초 이내 중복 타격 방지 (멀티플레이 중복 판정 2중 방어) ──
  if (DamageInstigator)
  {
      const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
      if (float* LastHitTime = RecentAttackerHitTimes.Find(DamageInstigator))
      {
          if ((CurrentTime - *LastHitTime) < 0.25f)
          {
              UE_LOG(LogTemp, Verbose, TEXT("[HitFilter] %s dropped hit from %s (%.3fs since last, dmg %.1f)"),
                     *GetName(), *GetNameSafe(DamageInstigator), CurrentTime - *LastHitTime, Amount);
              return; // 0.25초 이내 동일 공격자의 연속 타격 무시
          }
      }
      RecentAttackerHitTimes.Add(DamageInstigator, CurrentTime);
  }

  // 맞으면 전투 상태 진입/갱신
  EnterCombatState();

  // ── 0) I-Frame 또는 무적(Invincible) 우선 ─────────────────────────────────────────
  if (AbilitySystemComponent) {
    static const FGameplayTag Tag_IFrame = NonTags::State_IFrame.GetTag();
    static const FGameplayTag Tag_Invincible = NonTags::State_Invincible.GetTag();
    
    const bool bIFrame = AbilitySystemComponent->HasMatchingGameplayTag(Tag_IFrame);
    const bool bInvincible = AbilitySystemComponent->HasMatchingGameplayTag(Tag_Invincible);

    if (bIFrame || bInvincible) {
      if (HasAuthority()) {
        if (bInvincible) {
            Multicast_SpawnImmuneText(WorldLocation);
        } else {
            HandleIFrameDodge(DamageInstigator, WorldLocation);
        }
      }
      return; // 데미지 처리 중단!
    }
  }

  // 0 이하 무시
  if (Amount <= 0.f)
    return;

  // 🛡️ 피격 방향(Front / Back) C++ 자동 결합 기능
  FGameplayTag FinalReactionTag = ReactionTag;
  if (!FinalReactionTag.IsValid())
  {
      FinalReactionTag = NonTags::Effect_Hit_Light.GetTag();
  }

  // 공격자 위치 기반 전방 내적(Dot Product) 계산
  FVector AttackerLoc = DamageInstigator ? DamageInstigator->GetActorLocation() : WorldLocation;
  FVector DirToAttacker = (AttackerLoc - GetActorLocation()).GetSafeNormal2D();
  bool bFrontalHit = true;
  if (!DirToAttacker.IsNearlyZero()) {
      float Dot = FVector::DotProduct(GetActorForwardVector(), DirToAttacker);
      bFrontalHit = (Dot >= -0.2f);
  }

  // ReactionTag 에 Front/Back 이 없으면 C++ 이 피격 각도를 계산하여 .Front / .Back 자동 결합!
  // 🛡️ [Fix] Knockdown / Knockback 계열 태그는 방향 접미사(.Front/.Back)를 결합하지 않고 원본 보존!
  const FString TagStr = FinalReactionTag.ToString();
  const bool bIsKnockdownTag = TagStr.Contains(TEXT("Knockdown")) || TagStr.Contains(TEXT("KnockDown")) || TagStr.Contains(TEXT("Knockback"));
  
  if (!bIsKnockdownTag && !TagStr.Contains(TEXT(".Front")) && !TagStr.Contains(TEXT(".Back")))
  {
      FString SubSuffix = bFrontalHit ? TEXT(".Front") : TEXT(".Back");
      FGameplayTag CombinedTag = FGameplayTag::RequestGameplayTag(*FString::Printf(TEXT("%s%s"), *TagStr, *SubSuffix), false);
      if (CombinedTag.IsValid())
      {
          FinalReactionTag = CombinedTag;
      }
  }

  // 🛡️ 최근 피격 태그 저장 (GA_HitReaction 이 꺼내어 쓸 용도)
  LastHitReactionTag = FinalReactionTag;


  // 🛡️ 가드 중 피격 시: 전방 180도 범위일 때만 가드 성공! (등 뒤에서 맞으면 가드 뚫림!)
  // 가드 판정은 GuardComponent가 담당 (성공 시 피해 감소 + 가드 피격 몽타주)
  bool bParried = false;
  const bool bGuardBlocked = GuardComponent && GuardComponent->ResolveIncomingHit(Amount, bFrontalHit, ReactionTag, DamageInstigator, &bParried);

  // 패링 성공: 피해·피격 리액션 없음 (설계서 5장 16)
  if (bParried)
  {
      HandleParrySuccess(DamageInstigator, WorldLocation, bRangedHit);
      return;
  }

  if (!bGuardBlocked)
  {
      // 노가드 또는 가드 실패(등 뒤 피격) 시 GA_HitReaction 어빌리티 발송
      if (AbilitySystemComponent)
      {
          FGameplayEventData EventData;
          EventData.EventTag = FinalReactionTag;
          EventData.Instigator = DamageInstigator;
          EventData.Target = this;
          EventData.EventMagnitude = Amount;
          
          AbilitySystemComponent->HandleGameplayEvent(FinalReactionTag, &EventData);
      }
  }

  // ── 1) 정상 데미지(GAS 우선) ───────────────────────────────────
  if (AbilitySystemComponent && GE_Damage) {
    FGameplayEffectContextHandle Ctx =
        AbilitySystemComponent->MakeEffectContext();
    Ctx.AddInstigator(DamageInstigator,
                      Cast<APawn>(DamageInstigator)
                          ? Cast<APawn>(DamageInstigator)->GetController()
                          : nullptr);

    FHitResult HitResult;
    HitResult.Location = WorldLocation;
    HitResult.ImpactPoint = WorldLocation;
    Ctx.AddHitResult(HitResult);

    FGameplayEffectSpecHandle Spec =
        AbilitySystemComponent->MakeOutgoingSpec(GE_Damage, 1.f, Ctx);
    if (Spec.IsValid()) {
      static const FGameplayTag Tag_DataDamage =
          NonTags::Data_Damage.GetTag();
      Spec.Data->SetSetByCallerMagnitude(Tag_DataDamage, -Amount);

      // 🛡️ [Fix] FinalReactionTag 를 GE_Damage Spec 의 DynamicGrantedTags 에 심어주어
      // NonAttributeSet 이 중복으로 Effect.Hit.Light 기본 이벤트를 2중 발송하지 않도록 함!
      if (FinalReactionTag.IsValid())
      {
          Spec.Data->DynamicGrantedTags.AddTag(FinalReactionTag);
      }

      AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    }
  } else if (AbilitySystemComponent && AttributeSet) {
    const float Cur = AttributeSet->GetHP();
    const float Max = AttributeSet->GetMaxHP();
    const float NewVal = FMath::Clamp(Cur - Amount, 0.f, Max);
    AbilitySystemComponent->SetNumericAttributeBase(
        AttributeSet->GetHPAttribute(), NewVal);
  } else {
    // (선택) 완전 폴백
    UGameplayStatics::ApplyPointDamage(
        this, Amount, FVector::UpVector,
        FHitResult(WorldLocation, FVector::UpVector),
        DamageInstigator ? DamageInstigator->GetInstigatorController()
                         : nullptr,
        DamageInstigator ? DamageInstigator : this, UDamageType::StaticClass());
  }
}

// [New] 캐릭터 초기화 (직업/레벨 변경)
void ANonCharacterBase::InitCharacterData(EJobClass NewJob, int32 NewLevel) {
  UE_LOG(LogTemp, Verbose, TEXT("[JobTrace] %s InitCharacterData %d -> %d (NetMode %d, BegunPlay %d)"),
         *GetName(), (int32)DefaultJobClass, (int32)NewJob, (int32)GetNetMode(), HasActorBegunPlay() ? 1 : 0);
  DefaultJobClass = NewJob;

  // [Visual Update Test] - 이제 장비 기반으로 자동 처리하므로 강제 설정 제거
  // SetArmed(true);

  // [Legacy] 하드코딩된 스탠스 설정 제거 (이제 장비 아이템에 의해 자동 결정됨)
  /*
  if (NewJob == EJobClass::Defender) ...
  */

  RefreshWeaponStance();

  // [New] 장비 아이템도 실제로 변경
  EquipStartingItemsForJob(NewJob);

  // [Fix] 스킬 매니저의 직업 정보도 반드시 동기화해야 UI(스킬창)가 제대로
  // 갱신됨
  if (SkillMgr) {
    // DataAsset이 필요하다면 여기서 로드해서 넘겨야 하지만,
    // 일단 직업 타입이라도 맞춰야 함.
    SkillMgr->SetJobClass(NewJob);

    // 만약 DefaultDataAssets 맵이 SkillMgr 내부에 있다면 Init을 호출하는 게 더
    // 좋을 수 있음 지금은 SetJobClass(서버) -> Replicated -> Client UI 갱신
    // 흐름
  }

}

void ANonCharacterBase::EquipStartingItemsForJob(EJobClass Job) {
  if (!HasAuthority())
    return; // 아이템 생성은 서버 권한 필요

  UEquipmentComponent *EqComp = FindComponentByClass<UEquipmentComponent>();
  if (!EqComp)
    return;

  // 1. 기존 장비 해제 (모두)
  // (EqComp->UnequipAll() 같은게 있다면 좋겠지만, 없으면 생략하거나 하나씩
  // 해제)

  // 2. 해당 직업의 시작 아이템 목록 찾기
  const FStartingItemSet *FoundSet = JobStartingItems.Find(Job);
  if (!FoundSet)
    return;

  // 3. 아이템 생성 및 장착
  // [Fix] 아이템 생성을 위한 DataTable 로드
  UDataTable *DT = nullptr;
  if (UInventoryComponent *Inv = FindComponentByClass<UInventoryComponent>()) {
    DT = Inv->ItemDataTable;
  }

  for (TSubclassOf<UInventoryItem> ItemClass : FoundSet->Items) {
    if (ItemClass) {
      UInventoryItem *NewItem = NewObject<UInventoryItem>(this, ItemClass);
      if (NewItem) {
        // [Critical Fix] 서버에서도 Init을 호출하여 CachedRow를 DT에서 확실하게
        // 로드해야 함 (BP 기본값(CDO)이 유효하지 않거나 오래되었을 경우 대비)
        if (DT) {
          NewItem->Init(NewItem->ItemId, 1, DT);
        }

        // 바로 장착 시도
        EqComp->EquipItem(NewItem);

        // 만약 무기(Main)라도 일단은 납도 상태(비전투)로 시작
        // if (NewItem->GetEquipSlot() == EEquipmentSlot::WeaponMain)
        // {
        //     SetArmed(true);
        // }
      }
    }
  }

  // 4. 자세 갱신
  RefreshWeaponStance();
}

void ANonCharacterBase::Multicast_SpawnDamageNumber_Implementation(
    float Amount, FVector WorldLocation, bool bIsCritical) {
  if (Amount <= 0.f || !GetWorld())
    return; // 0이면 아무 것도 표시하지 않음 (안전장치)

  // ── [New] 원거리 데미지 숫자 가시거리 컬링 (25미터) ──
  // 내가 맞은 피해가 아니라면 로컬 카메라로부터 25m 이내일 때만 스폰
  if (!IsLocallyControlled())
  {
      if (APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0))
      {
          const float DistSq = FVector::DistSquared(CamMgr->GetCameraLocation(), WorldLocation);
          constexpr float MaxDamageDistSq = 2500.f * 2500.f; // 25미터 (2500cm)
          if (DistSq > MaxDamageDistSq)
          {
              return; // 너무 멀리서 일어난 타인의 데미지 숫자는 스폰하지 않음
          }
      }
  }

  // 폴백: 충돌 좌표가 0,0,0 이면 머리/가슴쯤에서 뜨게 보정
  if (WorldLocation.IsNearlyZero()) {
    if (USkeletalMeshComponent *Skel = GetMesh()) {
      // spine이나 head 소켓이 있으면 우선 사용
      const FName PrefSockets[] = {TEXT("head"), TEXT("spine_03"),
                                   TEXT("spine_02"), TEXT("spine_01")};
      bool bDone = false;
      for (const FName &S : PrefSockets) {
        if (Skel->DoesSocketExist(S)) {
          WorldLocation = Skel->GetSocketLocation(S) + FVector(0, 0, 20.f);
          bDone = true;
          break;
        }
      }
      if (!bDone) {
        const FBoxSphereBounds B = Skel->Bounds;
        WorldLocation = B.Origin + FVector(0, 0, B.BoxExtent.Z * 0.8f);
      }
    } else {
      WorldLocation = GetActorLocation() + FVector(0, 0, 100.f);
    }
  }

  TSubclassOf<ADamageNumberActor> SpawnClass = DamageNumberClass;
  if (!SpawnClass)
    SpawnClass = ADamageNumberActor::StaticClass();

  FActorSpawnParameters SP;
  SP.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  SP.Owner = this;

  if (ADamageNumberActor *A = GetWorld()->SpawnActor<ADamageNumberActor>(
          SpawnClass, WorldLocation, FRotator::ZeroRotator, SP)) {
    A->InitWithFlags(Amount, bIsCritical);
  }
}

void ANonCharacterBase::Multicast_SpawnPlayerDamageNumber_Implementation(float Amount, FVector WorldLocation)
{
  // 💡 [핵심] 플레이어 피격 데미지는 오직 맞은 본인의 화면에만 출력합니다!
  if (!IsLocallyControlled()) return;
  if (Amount <= 0.f || !GetWorld()) return;

  // 폴백 좌표 보정
  if (WorldLocation.IsNearlyZero()) {
    WorldLocation = GetActorLocation() + FVector(0, 0, 80.f);
  }

  TSubclassOf<ADamageNumberActor> SpawnClass = DamageNumberClass;
  if (!SpawnClass) SpawnClass = ADamageNumberActor::StaticClass();

  FActorSpawnParameters SP;
  SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  SP.Owner = this;

  if (ADamageNumberActor* A = GetWorld()->SpawnActor<ADamageNumberActor>(SpawnClass, WorldLocation, FRotator::ZeroRotator, SP))
  {
    A->Init(Amount, ENonDamageNumberCategory::PlayerDamage);
    A->SetOwner(this);
  }
}

void ANonCharacterBase::Multicast_SpawnHealNumber_Implementation(float Amount, FVector WorldLocation)
{
    TSubclassOf<ADamageNumberActor> SpawnClass = DamageNumberClass;
    if (!SpawnClass) SpawnClass = ADamageNumberActor::StaticClass();

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SP.Owner = this;

    const FVector SpawnLoc = WorldLocation + FVector(0.f, 0.f, 80.f);
    if (ADamageNumberActor* A = GetWorld()->SpawnActor<ADamageNumberActor>(SpawnClass, SpawnLoc, FRotator::ZeroRotator, SP))
    {
        A->Init(Amount, ENonDamageNumberCategory::Heal);
        A->SetOwner(this);
    }
}

void ANonCharacterBase::Multicast_SpawnDodgeText_Implementation(
    FVector WorldLocation) {
  UWorld *W = GetWorld();
  if (!W)
    return;

  TSubclassOf<ADamageNumberActor> SpawnClass = DamageNumberClass;
  if (!SpawnClass)
    SpawnClass = ADamageNumberActor::StaticClass();

  if (WorldLocation.IsNearlyZero()) {
    WorldLocation = GetActorLocation() + FVector(0, 0, 80.f);
  }

  FActorSpawnParameters P;
  P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  P.Owner = this;

  ADamageNumberActor *A = W->SpawnActor<ADamageNumberActor>(
      SpawnClass, WorldLocation, FRotator::ZeroRotator, P);
  if (A) {
    A->SetupAsDodge();
    A->SetOwner(this);
  }
}

void ANonCharacterBase::NotifyDodgeIFrameStart() {
  if (!HasAuthority())
    return;
  const UWorld *W = GetWorld();
  DodgeIFrameStartTime = W ? W->GetTimeSeconds() : -1.f;
  bJustDodgeUsedThisIFrame = false;
}

void ANonCharacterBase::HandleIFrameDodge(AActor *Attacker, const FVector &WorldLocation) {
  if (!HasAuthority())
    return;

  Multicast_SpawnDodgeText(WorldLocation);

  const UWorld *W = GetWorld();
  if (!W || DodgeIFrameStartTime < 0.f || bJustDodgeUsedThisIFrame)
    return;

  const float Elapsed = W->GetTimeSeconds() - DodgeIFrameStartTime;
  if (Elapsed > JustDodgeWindow)
    return;

  // 저스트 회피 성공 (회피 1번에 1회)
  bJustDodgeUsedThisIFrame = true;

  // 반격 인정 창: JustDodgeCounterWindow 동안 State.Evade.JustDodge (서버 전용).
  // 이 안에 보스를 처음 맞히면 보스가 그로기를 추가로 쌓고 태그를 소모한다.
  if (AbilitySystemComponent && JustDodgeCounterWindow > 0.f) {
    static const FGameplayTag Tag_JustCounter =
        NonTags::State_Evade_JustDodge.GetTag();
    if (Tag_JustCounter.IsValid()) {
      AbilitySystemComponent->SetLooseGameplayTagCount(Tag_JustCounter, 1);
      TWeakObjectPtr<ANonCharacterBase> WeakThis(this);
      GetWorldTimerManager().SetTimer(
          JustDodgeCounterTimer,
          FTimerDelegate::CreateLambda([WeakThis]() {
            if (WeakThis.IsValid() && WeakThis->AbilitySystemComponent) {
              WeakThis->AbilitySystemComponent->SetLooseGameplayTagCount(Tag_JustCounter, 0);
            }
          }),
          JustDodgeCounterWindow, false);
    }
  }

  if (AbilitySystemComponent) {
    static const FGameplayTag Tag_JustDodge =
        NonTags::Event_Dodge_Just.GetTag();
    if (Tag_JustDodge.IsValid()) {
      FGameplayEventData EventData;
      EventData.EventTag = Tag_JustDodge;
      EventData.Instigator = Attacker; // 반격 대상 (회피공격기 룬 등)
      EventData.Target = this;
      AbilitySystemComponent->HandleGameplayEvent(Tag_JustDodge, &EventData);
    }
  }

  Multicast_PlayJustDodgeFX(GetActorLocation());

  UE_LOG(LogTemp, Log, TEXT("[JustDodge] %s just-dodged %s (%.3fs after i-frame start)"),
         *GetName(), *GetNameSafe(Attacker), Elapsed);
}

void ANonCharacterBase::Multicast_PlayJustDodgeFX_Implementation(FVector WorldLocation) {
  if (IsRunningDedicatedServer())
    return;
  UWorld *W = GetWorld();
  if (!W)
    return;
  if (JustDodgeEffect) {
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        W, JustDodgeEffect, WorldLocation, GetActorRotation(), FVector(JustDodgeEffectScale));
  }
  if (JustDodgeSound) {
    UGameplayStatics::PlaySoundAtLocation(W, JustDodgeSound, WorldLocation);
  }

  // 잔상 (각 화면에서 로컬 생성)
  if (AfterimageMaterial) {
    TSubclassOf<AJustDodgeAfterimage> Cls = AfterimageClass;
    if (!Cls)
      Cls = AJustDodgeAfterimage::StaticClass();
    FActorSpawnParameters P;
    P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (AJustDodgeAfterimage *Img = W->SpawnActor<AJustDodgeAfterimage>(Cls, GetActorTransform(), P)) {
      Img->InitFromActor(this, AfterimageMaterial, AfterimageColor, AfterimageLifetime);
    }
  }
}

void ANonCharacterBase::HandleParrySuccess(AActor *Attacker, const FVector &WorldLocation, bool bRangedHit) {
  if (!HasAuthority())
    return;

  Multicast_SpawnParryText(WorldLocation);
  // 원거리(투사체) 패링은 플레이어만 멈춤 (멀리서 쏜 캐릭터가 혼자 멈추면 어색함)
  Multicast_ParryHitStop(bRangedHit ? nullptr : Attacker);

  // 패링 패시브(디펜더 등)가 듣는 이벤트. Instigator = 패링당한 공격자
  if (AbilitySystemComponent) {
    static const FGameplayTag Tag_ParrySuccess =
        NonTags::Event_Parry_Success.GetTag();
    if (Tag_ParrySuccess.IsValid()) {
      FGameplayEventData EventData;
      EventData.EventTag = Tag_ParrySuccess;
      EventData.Instigator = Attacker;
      EventData.Target = this;
      AbilitySystemComponent->HandleGameplayEvent(Tag_ParrySuccess, &EventData);
    }
  }

  // 원거리(투사체) 패링: 쏜 적에게는 경직/그로기를 주지 않음 (멀리 있는 적이 화살이 막혔다고 경직되면 어색함)
  // 플레이어 쪽 패링 패시브(Event.Parry.Success)와 연출은 위에서 그대로 처리
  if (bRangedHit) {
    UE_LOG(LogTemp, Log, TEXT("[Parry] %s parried ranged hit from %s"), *GetName(), *GetNameSafe(Attacker));
    return;
  }

  // 패링당한 적 반응: 일반 몬스터는 패링 경직(Effect.Hit.Parried → GA_HitReaction, 공격 몽타주 끊김).
  // 보스는 경직 없이 그로기 게이지 누적 (게이지 미구현 — 추후)
  if (AEnemyCharacter *Enemy = Cast<AEnemyCharacter>(Attacker)) {
    if (!Enemy->IsDead() && !Enemy->IsBoss()) {
      if (UAbilitySystemComponent *EnemyASC = Enemy->GetAbilitySystemComponent()) {
        static const FGameplayTag Tag_Parried =
            NonTags::Effect_Hit_Parried.GetTag();
        if (Tag_Parried.IsValid()) {
          FGameplayEventData Payload;
          Payload.EventTag = Tag_Parried;
          Payload.Instigator = this;
          Payload.Target = Enemy;
          EnemyASC->HandleGameplayEvent(Tag_Parried, &Payload);
        }
      }
    }
  }

  // 보스: 경직 대신 그로기 게이지 누적 (설계서 5장 5)
  if (ABossCharacter *Boss = Cast<ABossCharacter>(Attacker)) {
    if (!Boss->IsDead()) {
      Boss->AddGroggy(Boss->GroggyFromParry, this);
    }
  }

  UE_LOG(LogTemp, Log, TEXT("[Parry] %s parried %s"), *GetName(), *GetNameSafe(Attacker));
}

void ANonCharacterBase::Multicast_SpawnParryText_Implementation(
    FVector WorldLocation) {
  UWorld *W = GetWorld();
  if (!W)
    return;

  if (WorldLocation.IsNearlyZero()) {
    WorldLocation = GetActorLocation() + FVector(0, 0, 80.f);
  }

  // "패링" 글자는 표시하지 않음 (불꽃 + 소리 + 적 경직으로 전달)
  // 패링 연출 (GuardComponent 설정값). 이펙트/사운드는 모든 화면, 카메라 흔들림은 패링한 본인만
  if (GuardComponent && !IsRunningDedicatedServer()) {
    const FVector FXLoc = WorldLocation;
    if (GuardComponent->ParryEffect) {
      UNiagaraFunctionLibrary::SpawnSystemAtLocation(
          W, GuardComponent->ParryEffect, FXLoc, GetActorRotation(),
          FVector(GuardComponent->ParryEffectScale));
    }
    if (GuardComponent->ParrySound) {
      UGameplayStatics::PlaySoundAtLocation(W, GuardComponent->ParrySound, FXLoc);
    }
    if (GuardComponent->ParryCameraShake && IsLocallyControlled()) {
      if (APlayerController *PC = Cast<APlayerController>(GetController())) {
        PC->ClientStartCameraShake(GuardComponent->ParryCameraShake,
                                   GuardComponent->ParryCameraShakeScale);
      }
    }
  }
}

void ANonCharacterBase::Multicast_SpawnImmuneText_Implementation(
    FVector WorldLocation) {
  UWorld *W = GetWorld();
  if (!W)
    return;

  TSubclassOf<ADamageNumberActor> SpawnClass = DamageNumberClass;
  if (!SpawnClass)
    SpawnClass = ADamageNumberActor::StaticClass();

  if (WorldLocation.IsNearlyZero()) {
    WorldLocation = GetActorLocation() + FVector(0, 0, 80.f);
  }

  FActorSpawnParameters P;
  P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  P.Owner = this;

  ADamageNumberActor *A = W->SpawnActor<ADamageNumberActor>(
      SpawnClass, WorldLocation, FRotator::ZeroRotator, P);
  if (A) {
    A->SetupAsImmune();
    A->SetOwner(this);
  }
}

void ANonCharacterBase::OnGotHit(float Damage, AActor *InstigatorActor,
                                 const FVector &ImpactPoint,
                                 FGameplayTag ReactionTag) {
  if (IsDead() || Damage <= 0.f)
    return;

  // 히트스톱
  ApplyHitStopSelf(0.25f, 0.07f);

  // 피격 시 스킬 연계 전량 해제 (기획 요구사항 반영)
  if (SkillMgr) {
    SkillMgr->ForceClearAllCombos();
  }
}

void ANonCharacterBase::Multicast_ParryHitStop_Implementation(AActor *Attacker) {
  if (!GuardComponent || GuardComponent->ParryHitStopDuration <= 0.f) {
    return;
  }
  const float Scale = FMath::Clamp(GuardComponent->ParryHitStopScale, 0.05f, 1.f);
  const float Duration = GuardComponent->ParryHitStopDuration;

  ApplyHitStopSelf(Scale, Duration);

  // 공격자: 장판처럼 캐릭터가 아닌 것이 넘어오면 그것을 만든 캐릭터를 멈춤 (원거리 패링은 Attacker가 null로 옴)
  AActor *Target = Attacker;
  if (Target && !Target->IsA<APawn>()) {
    if (APawn *InstigatorPawn = Target->GetInstigator()) {
      Target = InstigatorPawn;
    } else if (AActor *OwnerActor = Target->GetOwner()) {
      Target = OwnerActor;
    }
  }
  if (Target && Target != this) {
    Target->CustomTimeDilation = Scale;
    TWeakObjectPtr<AActor> WeakTarget = Target;
    FTimerHandle RestoreHandle;
    GetWorldTimerManager().SetTimer(
        RestoreHandle,
        FTimerDelegate::CreateWeakLambda(this, [WeakTarget]() {
          if (AActor *A = WeakTarget.Get()) {
            A->CustomTimeDilation = 1.f;
          }
        }),
        Duration, false);
  }
}

void ANonCharacterBase::ApplyHitStopSelf(float Scale, float Duration) {
  Scale = FMath::Clamp(Scale, 0.05f, 1.f);
  CustomTimeDilation = Scale;

  GetWorldTimerManager().ClearTimer(HitStopTimer);
  GetWorldTimerManager().SetTimer(
      HitStopTimer,
      FTimerDelegate::CreateWeakLambda(this, [this]() { CustomTimeDilation = 1.f; }),
      Duration, false);
}

void ANonCharacterBase::ApplyGroundPush(const FVector &Dir, float Distance, float Duration) {
  if (!HasAuthority() || Distance <= 0.f) {
    return;
  }
  FVector FlatDir(Dir.X, Dir.Y, 0.f);
  if (!FlatDir.Normalize()) {
    return;
  }
  const float SafeDuration = FMath::Max(0.02f, Duration);
  const float Speed = Distance / SafeDuration;

  ApplyGroundPushLocal(FlatDir, Speed, SafeDuration);
  // 원격 클라이언트가 조종하는 캐릭터는 본인 클라이언트에도 같은 힘을 걸어야 위치 보정(끊김)이 없음
  if (!IsLocallyControlled() && IsPlayerControlled()) {
    Client_ApplyGroundPush(FlatDir, Speed, SafeDuration);
  }
}

void ANonCharacterBase::Client_ApplyGroundPush_Implementation(FVector Dir, float Speed, float Duration) {
  ApplyGroundPushLocal(Dir, Speed, Duration);
}

void ANonCharacterBase::ApplyGroundPushLocal(const FVector &Dir, float Speed, float Duration) {
  UCharacterMovementComponent *Move = GetCharacterMovement();
  if (!Move) {
    return;
  }

  static const FName GroundPushName(TEXT("GroundPush"));
  Move->RemoveRootMotionSource(GroundPushName); // 연속 피격 시 겹치지 않게 이전 밀림 제거

  TSharedPtr<FRootMotionSource_ConstantForce> Src = MakeShared<FRootMotionSource_ConstantForce>();
  Src->InstanceName = GroundPushName;
  Src->AccumulateMode = ERootMotionAccumulateMode::Override;
  Src->Priority = 5;
  Src->Force = Dir * Speed;
  Src->Duration = Duration;
  Src->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
  Src->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
  Move->ApplyRootMotionSource(Src);
}

void ANonCharacterBase::ResetUprightRotation() {
  FRotator Rot = GetActorRotation();
  if (FMath::IsNearlyZero(Rot.Pitch) && FMath::IsNearlyZero(Rot.Roll)) {
    return;
  }
  Rot.Pitch = 0.f;
  Rot.Roll = 0.f;
  SetActorRotation(Rot, ETeleportType::TeleportPhysics);
}

void ANonCharacterBase::SetDrawSheatheMoving(bool bMoving) {
  if (HasAuthority()) {
    bDrawSheatheMoving = bMoving;
  }
}

const FAnimMontageInstance *ANonCharacterBase::GetActiveDrawSheatheMontageInstance() const {
  const USkeletalMeshComponent *MeshComp = GetMesh();
  const UAnimInstance *AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
  if (!AnimInst || DrawSheatheSlotName.IsNone()) {
    return nullptr;
  }
  // 블렌드 아웃 중인 인스턴스도 포함 (끝나는 순간에도 기본 이동 상태를 고정해야 걷기 다리가 안 섞임)
  for (const FAnimMontageInstance *Inst : AnimInst->MontageInstances) {
    if (Inst && Inst->Montage && Inst->Montage->IsValidSlot(DrawSheatheSlotName)) {
      return Inst;
    }
  }
  return nullptr;
}

void ANonCharacterBase::HandleMontageStartedForProxyRootMotion(UAnimMontage *Montage) {
  // 서버와 조종하는 본인은 발도 어빌리티가 제자리/이동에 맞춰 직접 처리
  if (!Montage || HasAuthority() || IsLocallyControlled() || DrawSheatheSlotName.IsNone()) {
    return;
  }
  if (!Montage->IsValidSlot(DrawSheatheSlotName)) {
    return;
  }
  USkeletalMeshComponent *MeshComp = GetMesh();
  UAnimInstance *AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
  if (FAnimMontageInstance *Inst = AnimInst ? AnimInst->GetActiveInstanceForMontage(Montage) : nullptr) {
    if (!Inst->IsRootMotionDisabled()) {
      Inst->PushDisableRootMotion();
    }
  }
}

void ANonCharacterBase::OnRep_AttachmentReplication() {
  Super::OnRep_AttachmentReplication();
  if (!GetAttachParentActor()) {
    ResetUprightRotation();
  }
}

bool ANonCharacterBase::HasZeroHP() const {
  return (AttributeSet && AttributeSet->GetHP() <= 0.f);
}

bool ANonCharacterBase::IsDead() const { return bDied || HasZeroHP(); }

bool ANonCharacterBase::CanJumpInternal_Implementation() const {
  // 사망 시 점프 차단
  if (IsDead()) {
    return false;
  }

  // 대화 중 또는 상점 열림 시 점프 차단
  if (bIsDialogueCameraActive || (UIManagerComponent && UIManagerComponent->IsMerchantShopOpen())) {
    return false;
  }

  // 공격/스킬 몽타주 재생 중 점프 차단
  if (GetMesh() && GetMesh()->GetAnimInstance() && GetMesh()->GetAnimInstance()->IsAnyMontagePlaying()) {
    return false;
  }

  // 상태이상 및 액션 중 점프 차단 (Knockdown, HitReacting, CrowdControl, Attack, Skill, Dodge, Guard 등)
  if (AbilitySystemComponent) {
    static const FGameplayTag KnockdownTag = NonTags::State_Knockdown.GetTag();
    static const FGameplayTag HitReactingTag = NonTags::State_HitReacting.GetTag();
    static const FGameplayTag CCTag = NonTags::State_CrowdControl.GetTag();
    static const FGameplayTag AttackTag = NonTags::State_Attack.GetTag();
    static const FGameplayTag SkillTag = NonTags::State_Skill.GetTag();
    static const FGameplayTag DodgeTag = NonTags::State_Dodge.GetTag();
    static const FGameplayTag GuardTag = NonTags::State_Guard.GetTag();

    if (AbilitySystemComponent->HasMatchingGameplayTag(KnockdownTag) ||
        AbilitySystemComponent->HasMatchingGameplayTag(HitReactingTag) ||
        AbilitySystemComponent->HasMatchingGameplayTag(CCTag) ||
        AbilitySystemComponent->HasMatchingGameplayTag(AttackTag) ||
        AbilitySystemComponent->HasMatchingGameplayTag(SkillTag) ||
        AbilitySystemComponent->HasMatchingGameplayTag(DodgeTag) ||
        AbilitySystemComponent->HasMatchingGameplayTag(GuardTag)) {
      return false;
    }
  }

  return Super::CanJumpInternal_Implementation();
}

// ── 사망·부활: 실제 처리는 DeathComponent ──
void ANonCharacterBase::HandleDeath() {
  if (DeathComponent) DeathComponent->HandleDeath();
}

void ANonCharacterBase::OnRep_DeathServerTime() {
  // 실시간 사망은 HP 변화 처리(HandleDeath)가 담당. 늦게 접속해서 이미 죽어 있던 플레이어만 여기서 처리
  if (DeathServerTime < 0.f || bDied || !DeathComponent || !GetWorld())
    return;

  const AGameStateBase *GS = GetWorld()->GetGameState();
  const float Now = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
  const UAnimMontage *DM = GetDeathMontage();
  const float Len = DM ? DM->GetPlayLength() : 0.f;
  if (Now - DeathServerTime < Len + 0.5f)
    return;

  DeathComponent->HandleDeath();
  DeathComponent->FreezeDeathPose();
}

void ANonCharacterBase::FreezeDeathPose() {
  if (DeathComponent) DeathComponent->FreezeDeathPose();
}

void ANonCharacterBase::ApplyResurrectionPenalty() {
  if (DeathComponent) DeathComponent->ApplyResurrectionPenalty();
}

void ANonCharacterBase::Revive(bool bInPlace) {
  if (DeathComponent) DeathComponent->Revive(bInPlace);
}

UAnimMontage* ANonCharacterBase::GetDeathMontage() const {
  return DeathComponent ? DeathComponent->GetDeathMontage() : nullptr;
}

UAnimMontage* ANonCharacterBase::GetReviveMontage() const {
  return DeathComponent ? DeathComponent->GetReviveMontage() : nullptr;
}

UAnimMontage* ANonCharacterBase::GetHitMontage(FGameplayTag HitTag) const
{
    EWeaponStance CurrentStance = GetWeaponStance();

    // 🛡️ [Fix 1] Knockdown 태그가 전달되었을 때는 방향 태그(.Front/.Back)로 덮어쓰지 않고 최우선 적용!
    FGameplayTag TargetTag = HitTag;
    const bool bIsKnockdown = HitTag.ToString().Contains(TEXT("Knockdown")) || HitTag.ToString().Contains(TEXT("Knockback"));
    if (!bIsKnockdown && LastHitReactionTag.IsValid())
    {
        FString LastTagStr = LastHitReactionTag.ToString();
        if (LastTagStr.Contains(TEXT(".Front")) || LastTagStr.Contains(TEXT(".Back")))
        {
            TargetTag = LastHitReactionTag;
        }
    }

    if (const FHitReactionStanceMap* StanceMap = StanceHitMontages.Find(CurrentStance))
    {
        // 1. 정확한 태그 100% 일치 탐색
        if (UAnimMontage* const* FoundMontage = StanceMap->Montages.Find(TargetTag))
        {
            return *FoundMontage;
        }

        // 2. Knockdown 인 경우 맵에서 Knockdown 포함 태그 100% 정밀 탐색!
        if (bIsKnockdown)
        {
            for (const auto& Pair : StanceMap->Montages)
            {
                if (Pair.Key.IsValid() && Pair.Value && Pair.Key.ToString().Contains(TEXT("Knockdown")))
                {
                    return Pair.Value;
                }
            }
        }

        // 3. 일반 양방향 태그 매칭
        for (const auto& Pair : StanceMap->Montages)
        {
            if (Pair.Key.IsValid() && Pair.Value)
            {
                if (TargetTag.MatchesTag(Pair.Key) || Pair.Key.MatchesTag(TargetTag))
                {
                    return Pair.Value;
                }
            }
        }

        // 4. 맵에 등록된 첫 번째 유효 몽타주 Fallback
        for (const auto& Pair : StanceMap->Montages)
        {
            if (Pair.Value)
            {
                return Pair.Value;
            }
        }
    }

    // 5. 전역 Fallback: 어떤 스탠스에서든 등록된 피격 몽타주가 있다면 반환
    for (const auto& StancePair : StanceHitMontages)
    {
        for (const auto& Pair : StancePair.Value.Montages)
        {
            if (Pair.Value) return Pair.Value;
        }
    }

    return nullptr;
}

EWeaponStance
ANonCharacterBase::ComputeStanceFromItem(const UInventoryItem *Item) const {
  if (!Item)
    return EWeaponStance::Unarmed;

  const bool bIsStaff = (Item->CachedRow.WeaponType == EWeaponType::Staff);
  const bool bTwoHand = Item->IsTwoHandedWeapon() || bIsStaff;

  if (bIsStaff)
    return EWeaponStance::Staff;
  if (bTwoHand)
    return EWeaponStance::GreatSword;
  return EWeaponStance::SwordShield;
}

void ANonCharacterBase::SyncEquippedStanceFromEquipment() {
  UInventoryItem *Main = nullptr;
  if (!EquipmentComp) {
    EquipmentComp = FindComponentByClass<UEquipmentComponent>();
  }
  if (EquipmentComp) {
    Main = EquipmentComp->GetEquippedItemBySlot(EEquipmentSlot::WeaponMain);
  }

  // 메인 무기가 있으면 그 무기 기준으로, 없으면 기본값 유지
  EWeaponStance NewEquipped = ComputeStanceFromItem(Main);

  if (NewEquipped != EWeaponStance::Unarmed || (Main == nullptr)) {
    if (CachedArmedStance != NewEquipped) {
      CachedArmedStance = NewEquipped;
    }
  }
}

bool ANonCharacterBase::IsAnyMontagePlaying() const {
  if (const USkeletalMeshComponent *MeshComp = GetMesh())
    if (const UAnimInstance *AI = MeshComp->GetAnimInstance())
      return AI->IsAnyMontagePlaying();
  return false;
}

// Enum
// 0:Fwd, 1:Bwd, 2:Left, 3:Right, 4:FL, 5:FR, 6:BL, 7:BR (GA_Dodge와 순서 맞춰야
// 함) GA_Dodge.h의 EDodgeDirection 순서: F, B, L, R, FL, FR, BL, BR 0, 1, 2, 3,
// 4, 5, 6, 7

int32 ANonCharacterBase::GetDesiredDodgeDirection() const {
  // 입력 없으면
  FVector MoveInput = GetLastMovementInputVector();
  if (MoveInput.IsNearlyZero()) {
    // 0: Forward (Default)
    return 0;
  }

  FRotator ControlRot = GetControlRotation();
  ControlRot.Pitch = 0.f;
  ControlRot.Roll = 0.f;
  FVector Forward = ControlRot.Vector();
  FVector Right = FRotationMatrix(ControlRot).GetScaledAxis(EAxis::Y);

  float FwdDot = FVector::DotProduct(MoveInput, Forward);
  float RightDot = FVector::DotProduct(MoveInput, Right);

  bool bFwd = (FwdDot > 0.5f);
  bool bBwd = (FwdDot < -0.5f);
  bool bRight = (RightDot > 0.5f);
  bool bLeft = (RightDot < -0.5f);

  if (bFwd && bRight)
    return 5; // FR
  if (bFwd && bLeft)
    return 4; // FL
  if (bBwd && bRight)
    return 7; // BR
  if (bBwd && bLeft)
    return 6; // BL
  if (bFwd)
    return 0; // F
  if (bBwd)
    return 1; // B
  if (bLeft)
    return 2; // L
  if (bRight)
    return 3; // R

  return 0;
}

void ANonCharacterBase::TryDodge() {
  if (!AbilitySystemComponent)
    return;

  // 1. 방향 결정 (Client)
  int32 DirIndex = GetDesiredDodgeDirection();
  TempDodgeDirection = DirIndex;

  // 현재 카메라(컨트롤) Yaw 캡처
  float CurrentYaw = GetControlRotation().Yaw;

  // 2. 서버에 알림 (방향 인덱스 + 현재 회전값)
  ServerActivateDodge(DirIndex, CurrentYaw);

  // 3. 로컬 발동 (Prediction)
  static const FGameplayTag DodgeAbilityTag =
      NonTags::Ability_Dodge.GetTag();
  FGameplayTagContainer DodgeTagContainer;
  DodgeTagContainer.AddTag(DodgeAbilityTag);
  AbilitySystemComponent->TryActivateAbilitiesByTag(DodgeTagContainer);
}

void ANonCharacterBase::ServerActivateDodge_Implementation(int32 DirIndex,
                                                           float CurrentYaw) {
  // 서버측 상태 업데이트
  TempDodgeDirection = DirIndex;

  // [Critical Fix] 서버의 캐릭터 회전을 클라이언트가 보던 방향으로 강제 동기화
  // 이렇게 해야 '전방 회피'가 클라/서버에서 같은 월드 방향으로 나감.
  FRotator NewRot = GetActorRotation();
  NewRot.Yaw = CurrentYaw;
  SetActorRotation(NewRot);

  // 서버측 발동
  static const FGameplayTag DodgeAbilityTag =
      NonTags::Ability_Dodge.GetTag();
  if (AbilitySystemComponent) {
    FGameplayTagContainer DodgeTagContainer;
    DodgeTagContainer.AddTag(DodgeAbilityTag);
    AbilitySystemComponent->TryActivateAbilitiesByTag(DodgeTagContainer);
  }
}

void ANonCharacterBase::ClientOnAttackHitEnemy_Implementation(
    AEnemyCharacter *TargetEnemy) {
  if (TargetEnemy) {
    // 적의 로컬 HP바 표시 함수 호출
    TargetEnemy->ShowLocalHPBar(5.0f); // 5초간 표시

    // 클라이언트에서도 마지막으로 때린 적을 상단 타겟 HUD 대상으로 설정
    // (NonAttributeSet의 설정은 서버에서만 실행되어 원격 클라이언트에는 적용되지 않음)
    SetCombatTarget(TargetEnemy);
  }
}

void ANonCharacterBase::StartAttackAlignToCamera() {
  if (FollowCamera) {
    AttackAlignTargetRot =
        FRotator(0.f, FollowCamera->GetComponentRotation().Yaw, 0.f);
    bAttackAlignActive = true;
  }
}

void ANonCharacterBase::UpdateAttackAlign(float DeltaSeconds) {
  if (!bAttackAlignActive)
    return;

  const FRotator Current = GetActorRotation();

  // 부드러운 회전 (Tick마다 호출)
  const FRotator NewRot =
      FMath::RInterpTo(Current, AttackAlignTargetRot, DeltaSeconds,
                       AttackAlignSpeed // BP에서 15~30 정도로 튜닝
      );

  SetActorRotation(NewRot);

  const float DeltaYaw = FMath::Abs(
      FMath::FindDeltaAngleDegrees(NewRot.Yaw, AttackAlignTargetRot.Yaw));

  // 어느 정도 맞춰지면 정렬 종료
  if (DeltaYaw < 1.f) {
    bAttackAlignActive = false;
  }
}

void ANonCharacterBase::SetForceFullBody(bool bEnable) {
  if (bEnable) {
    // 풀바디 요청 추가
    ++ForceFullBodyRequestCount;
  } else {
    // 풀바디 요청 해제
    ForceFullBodyRequestCount = FMath::Max(0, ForceFullBodyRequestCount - 1);
  }

  // 하나라도 요청이 남아 있으면 true
  bForceFullBody = (ForceFullBodyRequestCount > 0);
}

// 전투 이벤트(공격/피격/어그로 발생 시 호출)
void ANonCharacterBase::EnterCombatState() {
  // 서버에서만 처리 (권장)
  if (!HasAuthority() || !AbilitySystemComponent)
    return;

  static const FGameplayTag CombatTag =
      NonTags::State_Combat.GetTag();
  static const FGameplayTag InCombatTag =
      NonTags::State_InCombat.GetTag();

  // 서버에서 붙이는 태그이므로 클라이언트에도 복제해야 클라에서의 물약/UI 판정이 맞음
  constexpr EGameplayTagReplicationState RepState = EGameplayTagReplicationState::TagAndCountToAll;
  if (CombatTag.IsValid()) {
    AbilitySystemComponent->SetLooseGameplayTagCount(CombatTag, 1, RepState);
  }
  if (InCombatTag.IsValid()) {
    AbilitySystemComponent->SetLooseGameplayTagCount(InCombatTag, 1, RepState);
  }

  GetWorldTimerManager().ClearTimer(CombatStateTimerHandle);
  GetWorldTimerManager().SetTimer(CombatStateTimerHandle, this,
                                  &ANonCharacterBase::LeaveCombatState,
                                  CombatStateDuration, false);
}

void ANonCharacterBase::LeaveCombatState() {
  if (!HasAuthority() || !AbilitySystemComponent)
    return;

  static const FGameplayTag CombatTag =
      NonTags::State_Combat.GetTag();
  static const FGameplayTag InCombatTag =
      NonTags::State_InCombat.GetTag();

  constexpr EGameplayTagReplicationState RepState = EGameplayTagReplicationState::TagAndCountToAll;
  if (CombatTag.IsValid()) {
    AbilitySystemComponent->SetLooseGameplayTagCount(CombatTag, 0, RepState);
  }
  if (InCombatTag.IsValid()) {
    AbilitySystemComponent->SetLooseGameplayTagCount(InCombatTag, 0, RepState);
  }

  GetWorldTimerManager().ClearTimer(CombatStateTimerHandle);
}

bool ANonCharacterBase::IsInCombat() const {
  if (!AbilitySystemComponent)
    return false;
  static const FGameplayTag CombatTag =
      NonTags::State_Combat.GetTag();
  static const FGameplayTag InCombatTag =
      NonTags::State_InCombat.GetTag();
  return (CombatTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(CombatTag)) ||
         (InCombatTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(InCombatTag));
}

void ANonCharacterBase::SaveGameTest() {
  if (USaveGameSubsystem *SaveSys =
          GetGameInstance()->GetSubsystem<USaveGameSubsystem>()) {
    SaveSys->SaveGame();
  }
}

void ANonCharacterBase::ResetGameTest() {
  if (USaveGameSubsystem *SaveSys =
          GetGameInstance()->GetSubsystem<USaveGameSubsystem>()) {
    SaveSys->DeleteSaveGame();
  }
}

// [New] 디버그 레벨업 구현 - LevelUp()을 호출하여 정상 레벨업과 동일하게 동작
void ANonCharacterBase::ServerDebugLevelUp_Implementation() {
  // [Changed] 기존 별도 로직 대신 LevelUp() 호출 (HUD 업데이트 포함)
  LevelUp();
}

// ==== 시네마틱 대화 카메라 ====
void ANonCharacterBase::BeginDialogueCamera(AActor* TargetNPC) {
  APlayerController* PC = Cast<APlayerController>(Controller);
  if (!PC || !TargetNPC || bIsDialogueCameraActive) return;

  bIsDialogueCameraActive = true;
  DialogueTargetNPC = TargetNPC;

  // 1. 카메라 액터 생성 (없으면 만들기)
  if (!DialogueCameraActor) {
      FActorSpawnParameters SpawnParams;
      SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
      DialogueCameraActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), GetActorLocation(), GetActorRotation(), SpawnParams);
      
      // 화면 양옆에 생기는 검은 띠(레터박스) 강제 비율 고정 해제
      if (UCameraComponent* CamComp = DialogueCameraActor->GetCameraComponent()) {
          CamComp->bConstrainAspectRatio = false;
      }
  }

  // 2. [New] NPC의 앞 방향(Forward)과 눈높이를 기준으로 정면 클로즈업 좌표 연산
  FVector NPCForward = TargetNPC->GetActorForwardVector();
  
  // NPC 정면 앞 140cm 및 눈높이(Z축 60cm) 보정
  FVector CameraLoc = TargetNPC->GetActorLocation() + (NPCForward * 140.f) + FVector(0.f, 0.f, 60.f);
  
  // NPC의 얼굴 부근(Z축 50cm 높이)을 바라보는 방향 각도(Rotator) 계산
  FVector NPCFaceLoc = TargetNPC->GetActorLocation() + FVector(0.f, 0.f, 50.f);
  FRotator CameraRot = FRotationMatrix::MakeFromX(NPCFaceLoc - CameraLoc).Rotator();

  DialogueCameraActor->SetActorLocationAndRotation(CameraLoc, CameraRot);

  // 3. 영화 같은 부드러운 카메라 뷰 전환 (Cubic 곡선 사용) - 시점 변환 제거를 위해 주석 처리
  // PC->SetViewTargetWithBlend(DialogueCameraActor, DialogueCameraBlendTime, EViewTargetBlendFunction::VTBlend_Cubic);

  // 4. 플레이어도 NPC를 자연스럽게 바라보게 정렬 회전
  FVector PlayerToNPC = (TargetNPC->GetActorLocation() - GetActorLocation());
  PlayerToNPC.Z = 0.f;
  FVector ForwardDir = PlayerToNPC.GetSafeNormal();
  FRotator PlayerTargetRot = ForwardDir.Rotation();
  PlayerTargetRot.Pitch = 0.f;
  PlayerTargetRot.Roll = 0.f;
  
  // 기존 공격 정렬 변수를 활용하여 플레이어 몸 방향 정렬
  AttackAlignTargetRot = PlayerTargetRot;
  bAttackAlignActive = true;
}

void ANonCharacterBase::EndDialogueCamera() {
  if (bIsDialogueCameraActive) {
    
    bIsDialogueCameraActive = false;
    
    // 다시 플레이어 자신(SpringArm/FollowCamera)으로 뷰를 스르륵 전환
    if (ANonPlayerController* PC = Cast<ANonPlayerController>(Controller)) {
        // PC->SetViewTargetWithBlend(this, DialogueCameraBlendTime, EViewTargetBlendFunction::VTBlend_Cubic);
        
        // [New] 대화 종료 후 상호작용 프롬프트가 즉시 뜨지 않도록 쿨다운 시작 (1.0초)
        PC->StartDialogueCooldown(1.0f);
    }

    // 대화창 UI 숨기기
    if (UNonUIManagerComponent* UIManager = FindComponentByClass<UNonUIManagerComponent>()) {
        UIManager->HideDialogue();
    }
  }
}

void ANonCharacterBase::ResetDialogueNPCRotation()
{
    if (ANPCCharacter* NPC = Cast<ANPCCharacter>(DialogueTargetNPC.Get()))
    {
        NPC->ReturnToOriginalRotation();
    }
    DialogueTargetNPC = nullptr;
}

void ANonCharacterBase::BindASCDelegates()
{
    // 스턴 태그 디버그 로그용 바인딩이 있던 자리. 이전에 등록된 바인딩만 정리
    if (AbilitySystemComponent)
    {
        FGameplayTag StunTag = NonTags::State_Stunned.GetTag();
        if (StunTag.IsValid())
        {
            AbilitySystemComponent->RegisterGameplayTagEvent(StunTag, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
        }
    }
}

void ANonCharacterBase::Interact_Implementation(ANonCharacterBase* Interactor)
{
    if (!Interactor || Interactor == this) return;

    // 상호작용한 플레이어의 Controller를 찾아 플레이어 상호작용 메뉴 팝업 요청
    if (ANonPlayerController* InteractorPC = Cast<ANonPlayerController>(Interactor->GetController()))
    {
        InteractorPC->ShowPlayerInteractionMenu(this);
    }
}

FText ANonCharacterBase::GetInteractLabel_Implementation()
{
    return FText::FromString(TEXT("상호작용"));
}

void ANonCharacterBase::SetInteractHighlight_Implementation(bool bEnable)
{
    // NPC와 동일하게 1번 스텐실(주황색 외곽선) 적용 및 캐릭터 전체 부위 메쉬에 일괄 적용
    TArray<USkeletalMeshComponent*> SkelMeshes;
    GetComponents<USkeletalMeshComponent>(SkelMeshes);
    for (USkeletalMeshComponent* MeshComp : SkelMeshes)
    {
        if (MeshComp)
        {
            MeshComp->SetRenderCustomDepth(bEnable);
            MeshComp->SetCustomDepthStencilValue(bEnable ? 1 : 0);
        }
    }
}

void ANonCharacterBase::SetPlayerName(const FString &NewName)
{
    PlayerName = NewName;
    UpdateOverheadName(NewName);

    if (IsLocallyControlled())
    {
        if (UNonUIManagerComponent* UIMgr = FindComponentByClass<UNonUIManagerComponent>())
        {
            UIMgr->UpdateCharacterName(NewName);
        }
    }
}

void ANonCharacterBase::OnRep_PlayerName()
{
    UpdateOverheadName(PlayerName);

    if (IsLocallyControlled())
    {
        if (UNonUIManagerComponent* UIMgr = FindComponentByClass<UNonUIManagerComponent>())
        {
            UIMgr->UpdateCharacterName(PlayerName);
        }
    }
}

void ANonCharacterBase::UpdateOverheadName(const FString& InName)
{
    if (!OverheadNameWidget) return;

    // 본인 캐릭터인 경우 bShowMyOverheadName 옵션(기본값 false)에 따라 숨김/표시 제어
    if (IsLocallyControlled())
    {
        OverheadNameWidget->SetVisibility(bShowMyOverheadName);
        if (!bShowMyOverheadName)
        {
            return; // 본인 머리 위 이름은 숨김
        }
    }
    else
    {
        OverheadNameWidget->SetVisibility(true); // 타인 캐릭터는 머리 위 이름 표시
    }

    // 위젯 클래스는 BP Class Defaults의 OverheadNameWidgetClass (없으면 컴포넌트에 직접 지정된 클래스) 사용
    if (OverheadNameWidgetClass && OverheadNameWidget->GetWidgetClass() != OverheadNameWidgetClass)
    {
        OverheadNameWidget->SetWidgetClass(OverheadNameWidgetClass);
    }
    if (!OverheadNameWidget->GetWidgetClass())
    {
        static bool bWarnedOverhead = false;
        if (!bWarnedOverhead)
        {
            bWarnedOverhead = true;
            UE_LOG(LogTemp, Warning, TEXT("[OverheadName] %s: OverheadNameWidgetClass가 비어 있습니다. BP Class Defaults에서 WBP_NPCName을 지정하세요"), *GetClass()->GetName());
        }
        return;
    }

    OverheadNameWidget->InitWidget();

    if (UUserWidget* WidgetObj = OverheadNameWidget->GetUserWidgetObject())
    {
        if (UTextBlock* NameText = Cast<UTextBlock>(WidgetObj->GetWidgetFromName(TEXT("Text_PlayerName"))))
        {
            NameText->SetText(FText::FromString(InName));
            return;
        }
        if (UTextBlock* NameText = Cast<UTextBlock>(WidgetObj->GetWidgetFromName(TEXT("Text_NPCName"))))
        {
            NameText->SetText(FText::FromString(InName));
            return;
        }
    }
}

void ANonCharacterBase::SetShowMyOverheadName(bool bShow)
{
    bShowMyOverheadName = bShow;
    if (IsLocallyControlled() && OverheadNameWidget)
    {
        OverheadNameWidget->SetVisibility(bShow);
        if (bShow)
        {
            UpdateOverheadName(PlayerName);
        }
    }
}

UInventoryComponent* ANonCharacterBase::GetInventoryComponent() const
{
    if (InventoryComp) return InventoryComp;
    return FindComponentByClass<UInventoryComponent>();
}



// ── [Combat|Guard|Turn] 가드 중 제자리 턴(Step Turn) 시스템 구현 ──

void ANonCharacterBase::UpdateGuardTurnInPlace(float DeltaSeconds) {
  if (!IsGuarding()) {
    bIsGuardTurning = false;
    ActiveGuardTurnMontage = nullptr;
    GuardTurnPendingTime = 0.f;
    return;
  }

  // 쿨다운 진행 중
  if (GuardTurnCooldown > 0.f) {
    GuardTurnCooldown -= DeltaSeconds;
  }

  const FRotator CurrentActorRot = GetActorRotation();
  const FRotator ControlRot = GetControlRotation();

  // 컨트롤러(카메라)와 액터 간의 최단 Yaw 각도 차이 (-180 ~ +180)
  const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentActorRot.Yaw, ControlRot.Yaw);
  const float AbsDeltaYaw = FMath::Abs(DeltaYaw);

  UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
  if (!AnimInst)
    return;

  // ── [1단계] 턴 몽타주가 현재 재생 중인 경우 ──
  if (bIsGuardTurning) {
    GuardTurnElapsed += DeltaSeconds;
    const float TotalDuration = FMath::Max(0.2f, GuardTurnMontageDuration);
    const float Progress = FMath::Clamp(GuardTurnElapsed / TotalDuration, 0.f, 1.0f);

    // 마우스를 계속 움직이고 있다면 목표 각도를 카메라 방향으로 동적 추종
    TargetGuardTurnYaw = ControlRot.Yaw;

    // 모션 워핑 타겟을 실시간 카메라 각도로 갱신
    if (MotionWarpingComp) {
      FRotator WarpRot = FRotator(0.f, TargetGuardTurnYaw, 0.f);
      MotionWarpingComp->AddOrUpdateWarpTargetFromTransform(GuardTurnWarpTargetName, FTransform(WarpRot, GetActorLocation()));
    }

    // [Upgrade 기능]: 90도 턴이 시작되었으나 초반 0.25초 내에 마우스를 더 돌려 뒤(115도 이상)를 보게 된 경우
    // 즉시 180도 턴 몽타주로 승격(Upgrade)하여 끊김 없이 180도 턴 수행!
    if (GuardTurnElapsed <= 0.25f && AbsDeltaYaw >= GuardTurn180ThresholdAngle) {
      UAnimMontage* Desired180 = (DeltaYaw < 0.f) ? GuardTurnLeft180Montage : GuardTurnRight180Montage;
      if (Desired180 && ActiveGuardTurnMontage != Desired180) {
        ActiveGuardTurnMontage = Desired180;
        GuardTurnMontageDuration = PlayAnimMontage(Desired180);
        GuardTurnElapsed = 0.f;

        if (MotionWarpingComp) {
          FRotator WarpRot = FRotator(0.f, TargetGuardTurnYaw, 0.f);
          MotionWarpingComp->AddOrUpdateWarpTargetFromTransform(GuardTurnWarpTargetName, FTransform(WarpRot, GetActorLocation()));
        }

        if (!HasAuthority()) {
          Server_PlayGuardTurn(Desired180, TargetGuardTurnYaw);
        } else {
          Multicast_PlayGuardTurn(Desired180, TargetGuardTurnYaw);
        }
      }
    }

    // [최단 각도 회전 보간]: FindDeltaAngleDegrees를 사용하여 180도 경계에서 360도 대회전 버그 원천 해결
    const float DeltaAngleToTarget = FMath::FindDeltaAngleDegrees(CurrentActorRot.Yaw, TargetGuardTurnYaw);

    // 몽타주가 루트 모션을 사용하는지 확인 (루트 모션 활성화 시 모터/엔진이 캡슐을 직접 회전시킴)
    const bool bHasRootMotion = IsPlayingRootMotion()
        || (AnimInst && AnimInst->GetRootMotionMontageInstance() != nullptr)
        || (ActiveGuardTurnMontage && ActiveGuardTurnMontage->HasRootMotion());

    if (!bHasRootMotion) {
      // [인플레이스 모드]: 몽타주 재생 시간에 맞춘 묵직하고 자연스러운 S-커브 회전
      // 0.00 ~ 0.08초: 발 들림 시작 (회전 0% 유지)
      // 0.08 ~ 0.88초: 몽타주 발 디딤 모션과 1:1로 일치하는 주 회전 (충분한 회전 시간 확보로 짧은 느낌 완전 해소)
      // 0.88 ~ 1.00초: 목표 각도에 100% 안착한 상태에서 발 마무리 착지 (스냅 0%, 텔레포트 0%)
      float TurnWeight = 0.f;
      if (Progress <= 0.08f) {
        TurnWeight = 0.f;
      } else if (Progress <= 0.88f) {
        const float T = (Progress - 0.08f) / (0.88f - 0.08f);
        TurnWeight = FMath::SmoothStep(0.f, 1.f, T);
      } else {
        TurnWeight = 1.0f;
      }

      const float TotalTurnAngle = FMath::FindDeltaAngleDegrees(StartGuardTurnYaw, TargetGuardTurnYaw);
      const float DesiredYaw = FRotator::NormalizeAxis(StartGuardTurnYaw + (TotalTurnAngle * TurnWeight));

      FRotator NewRot = CurrentActorRot;
      NewRot.Yaw = DesiredYaw; // 속도 제한으로 인한 뒤처짐 및 턴 완료 시 툭 튀는 텔레포트 현상 원천 차단!
      SetActorRotation(NewRot);
    } else {
      // [루트 모션 및 모션 워핑 모드]:
      // 모션 워핑이 몽타주에 있으면 각도가 100% 일치하므로 보정이 필요 없으며,
      // 모션 워핑 미사용 시에도 30도씩 뒤로 역회전하며 '툭' 튀는 현상을 막기 위해 10도 이내 미세 오차만 부드럽게 정렬
      if (Progress >= 0.85f && FMath::Abs(DeltaAngleToTarget) > 0.5f && FMath::Abs(DeltaAngleToTarget) <= 10.0f) {
        const float Step = DeltaAngleToTarget * FMath::Clamp(DeltaSeconds * 8.f, 0.f, 1.f);
        FRotator NewRot = CurrentActorRot;
        NewRot.Yaw = FRotator::NormalizeAxis(CurrentActorRot.Yaw + Step);
        SetActorRotation(NewRot);
      }
    }

    // 몽타주 실제 재생 여부 확인
    const bool bMontageStillPlaying = ActiveGuardTurnMontage ? AnimInst->Montage_IsPlaying(ActiveGuardTurnMontage) : false;

    // 🌟 [핵심 수정]: 0.4초 조기 끊김 버그 원천 해결!
    // 몽타주가 1초짜리인데 0.4초 만에 끊기면 각도를 반(30°)밖에 못 돌아 남은 각도로 인해 2번 연속 발동됨.
    // 따라서 몽타주가 90% 이상(0.9초) 온전히 재생을 마쳤을 때만 정상 완료 처리!
    const bool bTimeExpired = (Progress >= 1.0f) || (GuardTurnElapsed >= (TotalDuration * 0.95f));
    const bool bMontageNaturallyEnded = (!bMontageStillPlaying && GuardTurnElapsed >= (TotalDuration * 0.85f));

    if (bTimeExpired || bMontageNaturallyEnded) {
      bIsGuardTurning = false;
      ActiveGuardTurnMontage = nullptr;
      GuardTurnCooldown = 0.35f; // 턴 완료 후 0.35초간 안정적 쿨다운으로 연타 발동 원천 차단

      if (MotionWarpingComp) {
        MotionWarpingComp->RemoveWarpTarget(GuardTurnWarpTargetName);
      }
    }
    return;
  }

  // ── [2단계] 턴 발동 여부 검사 (로컬 플레이어가 주도적으로 각도 감지 및 발동) ──
  if (!IsLocallyControlled()) {
    // 서버에서 리모트 캐릭터(시뮬레이티드 프록시) 회전 동기화
    if (HasAuthority() && bIsGuardTurning) {
      const float DeltaAngle = FMath::FindDeltaAngleDegrees(CurrentActorRot.Yaw, TargetGuardTurnYaw);
      const float Step = DeltaAngle * FMath::Clamp(DeltaSeconds * 16.f, 0.f, 1.f);
      FRotator NewRot = CurrentActorRot;
      NewRot.Yaw = FRotator::NormalizeAxis(CurrentActorRot.Yaw + Step);
      SetActorRotation(NewRot);
    }
    return;
  }

  // [잔여 속도 검사]: 캐릭터가 아직 멈추지 않고 걷거나 감속 중(속도 > 30cm/s)이거나 이동 입력이 있으면 턴 발동 금지!
  const float CurrentSpeed2D = GetVelocity().Size2D();
  const FVector InputVec = GetLastMovementInputVector();
  if (CurrentSpeed2D > 30.f || InputVec.SizeSquared() > 0.01f) {
    GuardTurnPendingTime = 0.f;
    return;
  }

  // 쿨다운 중에는 턴 발동 대기
  if (GuardTurnCooldown > 0.f) {
    GuardTurnPendingTime = 0.f;
    return;
  }

  // 턴 기준 각도(50도) 미만이면 발동하지 않음
  if (AbsDeltaYaw < GuardTurnThresholdAngle) {
    GuardTurnPendingTime = 0.f;
    return;
  }

  // [플릭 마우스 보호 버퍼]: 마우스를 휙 돌려 뒤(180도)를 보려고 할 때
  // 50도를 지나는 순간 90도 턴이 섣불리 발동하지 않도록:
  // - 이미 115도 이상이면 즉시 180도 턴 발동!
  // - 50도 ~ 115도 사이라면 0.08초 동안 마우스가 180도까지 더 돌아가는지 관찰 후 90도 턴 발동!
  UAnimMontage* MontageToPlay = nullptr;
  if (AbsDeltaYaw >= GuardTurn180ThresholdAngle) {
    MontageToPlay = (DeltaYaw < 0.f) ? GuardTurnLeft180Montage : GuardTurnRight180Montage;
    if (!MontageToPlay) {
      MontageToPlay = (DeltaYaw < 0.f) ? GuardTurnLeft90Montage : GuardTurnRight90Montage;
    }
    if (!MontageToPlay) {
      MontageToPlay = GuardTurnRight90Montage ? GuardTurnRight90Montage : GuardTurnLeft90Montage;
    }
    GuardTurnPendingTime = 0.f; // 180도는 지체 없이 즉시 발동
  } else {
    // 50도 ~ 115도 사이: 마우스 드래그가 180도로 이어지는지 0.08초 관찰
    GuardTurnPendingTime += DeltaSeconds;
    if (GuardTurnPendingTime < 0.08f) {
      return; // 마우스 이동 관찰 대기
    }
    MontageToPlay = (DeltaYaw < 0.f) ? GuardTurnLeft90Montage : GuardTurnRight90Montage;
    if (!MontageToPlay) {
      MontageToPlay = GuardTurnRight90Montage ? GuardTurnRight90Montage : GuardTurnLeft90Montage;
    }
    GuardTurnPendingTime = 0.f;
  }

  if (MontageToPlay) {
    const float PlayDuration = PlayAnimMontage(MontageToPlay);
    if (PlayDuration > 0.f) {
      bIsGuardTurning = true;
      ActiveGuardTurnMontage = MontageToPlay;
      StartGuardTurnYaw = CurrentActorRot.Yaw;
      TargetGuardTurnYaw = ControlRot.Yaw;

      // 모션 워핑 타겟을 실시간 카메라 각도로 갱신
      if (MotionWarpingComp) {
        FRotator WarpRot = FRotator(0.f, TargetGuardTurnYaw, 0.f);
        MotionWarpingComp->AddOrUpdateWarpTargetFromTransform(GuardTurnWarpTargetName, FTransform(WarpRot, GetActorLocation()));
      }

      GuardTurnMontageDuration = PlayDuration;
      GuardTurnElapsed = 0.f;
      GuardTurnCooldown = 0.f;

      // 서버 및 다른 클라이언트에 턴 동기화
      if (!HasAuthority()) {
        Server_PlayGuardTurn(MontageToPlay, TargetGuardTurnYaw);
      } else {
        Multicast_PlayGuardTurn(MontageToPlay, TargetGuardTurnYaw);
      }
    }
  }
}

void ANonCharacterBase::OnGuardTurnMontageEnded(UAnimMontage* Montage, bool bInterrupted) {
  if (bIsGuardTurning) {
    bIsGuardTurning = false;
    ActiveGuardTurnMontage = nullptr;
    GuardTurnCooldown = 0.2f;
  }

  if (MotionWarpingComp) {
    MotionWarpingComp->RemoveWarpTarget(GuardTurnWarpTargetName);
  }

  // 턴 종료 시 모션 워핑 타겟 해제 및 상태 정돈 (하드 스냅 제거로 부드러운 정지 보장)
}

void ANonCharacterBase::Server_PlayGuardTurn_Implementation(UAnimMontage* MontageToPlay, float TargetYaw) {
  TargetGuardTurnYaw = TargetYaw;
  bIsGuardTurning = true;
  GuardTurnElapsed = 0.f;
  if (MontageToPlay) {
    GuardTurnMontageDuration = MontageToPlay->GetPlayLength();
  }
  Multicast_PlayGuardTurn(MontageToPlay, TargetYaw);
}

void ANonCharacterBase::Multicast_PlayGuardTurn_Implementation(UAnimMontage* MontageToPlay, float TargetYaw) {
  if (IsLocallyControlled()) {
    return;
  }

  if (MontageToPlay) {
    bIsGuardTurning = true;
    ActiveGuardTurnMontage = MontageToPlay;
    StartGuardTurnYaw = GetActorRotation().Yaw;
    TargetGuardTurnYaw = TargetYaw;
    GuardTurnMontageDuration = MontageToPlay->GetPlayLength();
    GuardTurnElapsed = 0.f;

    if (MotionWarpingComp) {
      FRotator WarpRot = FRotator(0.f, TargetGuardTurnYaw, 0.f);
      MotionWarpingComp->AddOrUpdateWarpTargetFromTransform(GuardTurnWarpTargetName, FTransform(WarpRot, GetActorLocation()));
    }

    PlayAnimMontage(MontageToPlay);
  }
}
