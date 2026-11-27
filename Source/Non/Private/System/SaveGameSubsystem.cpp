#include "System/SaveGameSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Character/NonCharacterBase.h"
#include "Ability/NonAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Skill/SkillManagerComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryItem.h"
#include "Equipment/EquipmentComponent.h"
#include "UI/QuickSlot/QuickSlotManager.h"
#include "System/NonGameInstance.h"
#include "Core/NonUIManagerComponent.h" // [Fix] for RefreshHUDState


void USaveGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USaveGameSubsystem::AutoSaveTick), TickInterval);
    FWorldDelegates::OnWorldCleanup.AddUObject(this, &USaveGameSubsystem::OnWorldCleanup);
}

void USaveGameSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    FWorldDelegates::OnWorldCleanup.RemoveAll(this);
    Super::Deinitialize();
}

void USaveGameSubsystem::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
    // 내 게임의 맵이 내려가면 다음 맵에서 적용이 끝날 때까지 저장 중지
    if (World && World->GetGameInstance() == GetGameInstance())
    {
        SetSaveReady(false);
    }
}

void USaveGameSubsystem::SetSaveReady(bool bReady)
{
    if (bSaveReady == bReady) return;
    bSaveReady = bReady;
    LastCompareBytes.Reset();
    SaveSoonAt = -1.0;
    if (bReady)
    {
        CachedSave = LoadCharacterSave();
        bBackupDone = false;
        NextAutoCheckAt = FPlatformTime::Seconds() + AutoSaveInterval;
    }
    else
    {
        CachedSave = nullptr;
    }
    UE_LOG(LogTemp, Log, TEXT("[Save] 저장 %s"), bReady ? TEXT("시작 (세이브 적용 끝)") : TEXT("중지"));
}

FString USaveGameSubsystem::GetSlotName() const
{
    const UNonGameInstance* GI = Cast<UNonGameInstance>(GetGameInstance());
    return GI ? GI->GetCharacterSaveSlotName() : FString();
}

ANonCharacterBase* USaveGameSubsystem::GetLocalCharacter() const
{
    const UGameInstance* GI = GetGameInstance();
    APlayerController* PC = GI ? GI->GetFirstLocalPlayerController(GetWorld()) : nullptr;
    return PC ? Cast<ANonCharacterBase>(PC->GetPawn()) : nullptr;
}

UNonSaveGame* USaveGameSubsystem::LoadCharacterSave() const
{
    const FString Slot = GetSlotName();
    if (Slot.IsEmpty()) return nullptr;

    if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        if (UNonSaveGame* Save = Cast<UNonSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
        {
            return Save;
        }
        UE_LOG(LogTemp, Warning, TEXT("[Save] %s 파일을 읽지 못함 -> 백업에서 읽기"), *Slot);
    }

    const FString Backup = GetBackupSlotName(Slot);
    if (UGameplayStatics::DoesSaveGameExist(Backup, 0))
    {
        return Cast<UNonSaveGame>(UGameplayStatics::LoadGameFromSlot(Backup, 0));
    }
    return nullptr;
}

void USaveGameSubsystem::FillSaveData(UNonSaveGame* SaveInst, ANonCharacterBase* NonChar) const
{
    SaveInst->SaveVersion = NonSaveVersion::Current;

    // 좌표는 저장한 맵 이름과 함께 — 같은 맵으로 들어올 때만 사용
    SaveInst->PlayerTransform = NonChar->GetActorTransform();
    SaveInst->SavedMapName = FName(*UGameplayStatics::GetCurrentLevelName(NonChar, true));
    SaveInst->PlayerName = NonChar->GetPlayerName();

    if (const UNonAttributeSet* AS = Cast<UNonAttributeSet>(NonChar->GetAttributeSet()))
    {
        SaveInst->Level = static_cast<int32>(AS->GetLevel());
        SaveInst->EXP = static_cast<int32>(AS->GetExp());
        SaveInst->CurrentHP = AS->GetHP();
        SaveInst->CurrentMP = AS->GetMP();
    }

    if (USkillManagerComponent* SkillMgr = NonChar->FindComponentByClass<USkillManagerComponent>())
    {
        SaveInst->SkillPoints = SkillMgr->GetSkillPoints();
        SaveInst->JobClass = SkillMgr->GetJobClass();
        SaveInst->SkillLevels = SkillMgr->GetSkillLevelMap();
    }

    if (UInventoryComponent* Inven = NonChar->FindComponentByClass<UInventoryComponent>())
    {
        SaveInst->InventoryItems = Inven->GetItemsForSave();
        SaveInst->Gold = Inven->GetGold();
    }

    if (UEquipmentComponent* Equip = NonChar->FindComponentByClass<UEquipmentComponent>())
    {
        SaveInst->EquippedItems = Equip->GetEquippedItemsForSave();
    }

    if (UQuickSlotManager* QM = NonChar->GetQuickSlotManager())
    {
        SaveInst->QuickSlots = QM->GetQuickSlotsForSave();
    }
}

TArray<uint8> USaveGameSubsystem::MakeCompareBytes(UNonSaveGame* SaveInst) const
{
    // 위치·HP·MP는 계속 바뀌므로 비교에서 뺀다 (저장할 때는 같이 들어감)
    const FTransform KeepTransform = SaveInst->PlayerTransform;
    const float KeepHP = SaveInst->CurrentHP;
    const float KeepMP = SaveInst->CurrentMP;
    SaveInst->PlayerTransform = FTransform::Identity;
    SaveInst->CurrentHP = 0.f;
    SaveInst->CurrentMP = 0.f;

    TArray<uint8> Bytes;
    UGameplayStatics::SaveGameToMemory(SaveInst, Bytes);

    SaveInst->PlayerTransform = KeepTransform;
    SaveInst->CurrentHP = KeepHP;
    SaveInst->CurrentMP = KeepMP;
    return Bytes;
}

bool USaveGameSubsystem::WriteSave(ANonCharacterBase* Char, bool bOnlyIfChanged)
{
    if (!bSaveReady || !Char) return false;

    const FString Slot = GetSlotName();
    if (Slot.IsEmpty()) return false;

    if (!CachedSave)
    {
        CachedSave = Cast<UNonSaveGame>(UGameplayStatics::CreateSaveGameObject(UNonSaveGame::StaticClass()));
        if (!CachedSave) return false;
    }

    FillSaveData(CachedSave, Char);

    TArray<uint8> CompareBytes = MakeCompareBytes(CachedSave);
    if (bOnlyIfChanged && CompareBytes == LastCompareBytes)
    {
        return false;
    }

    // 맵에 들어와 처음 쓸 때 직전 파일을 백업으로 남김 (파일이 깨지거나 잘못 덮어써도 한 단계 되돌릴 수 있게)
    if (!bBackupDone)
    {
        TArray<uint8> OldBytes;
        if (UGameplayStatics::LoadDataFromSlot(OldBytes, Slot, 0) && OldBytes.Num() > 0)
        {
            UGameplayStatics::SaveDataToSlot(OldBytes, GetBackupSlotName(Slot), 0);
        }
        bBackupDone = true;
    }

    TArray<uint8> Bytes;
    const bool bSuccess = UGameplayStatics::SaveGameToMemory(CachedSave, Bytes)
        && UGameplayStatics::SaveDataToSlot(Bytes, Slot, 0);
    if (bSuccess)
    {
        LastCompareBytes = MoveTemp(CompareBytes);
    }
    UE_LOG(LogTemp, Log, TEXT("[Save] %s 저장 %s (%d bytes)"), *Slot, bSuccess ? TEXT("완료") : TEXT("실패"), Bytes.Num());
    OnGameSaved.Broadcast(bSuccess);
    return bSuccess;
}

bool USaveGameSubsystem::AutoSaveTick(float DeltaTime)
{
    if (!bSaveReady) return true;

    const double Now = FPlatformTime::Seconds();
    const bool bSoonDue = SaveSoonAt > 0.0 && Now >= SaveSoonAt;
    const bool bAutoDue = Now >= NextAutoCheckAt;
    if (!bSoonDue && !bAutoDue) return true;

    NextAutoCheckAt = Now + AutoSaveInterval;
    SaveSoonAt = -1.0;

    if (ANonCharacterBase* Char = GetLocalCharacter())
    {
        WriteSave(Char, /*bOnlyIfChanged*/ true);
    }
    return true;
}

void USaveGameSubsystem::RequestSaveSoon(float DelaySeconds)
{
    const double At = FPlatformTime::Seconds() + FMath::Max(0.f, DelaySeconds);
    SaveSoonAt = (SaveSoonAt > 0.0) ? FMath::Min(SaveSoonAt, At) : At;
}

void USaveGameSubsystem::SaveGame()
{
    if (!bSaveReady)
    {
        UE_LOG(LogTemp, Log, TEXT("[Save] 세이브 적용 전이라 저장 건너뜀"));
        return;
    }
    WriteSave(GetLocalCharacter(), /*bOnlyIfChanged*/ false);
}

void USaveGameSubsystem::SaveCharacter(ANonCharacterBase* Char)
{
    WriteSave(Char, /*bOnlyIfChanged*/ false);
}

void USaveGameSubsystem::LoadGame()
{
    UNonSaveGame* LoadInst = LoadCharacterSave();
    if (!LoadInst) return;

    // 로컬 플레이어 폰 찾기
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!PlayerPawn) return;

    // 1. 위치 복구: 저장한 맵과 지금 맵이 같을 때만 (다른 맵 좌표로 벽 속·허공에 떨어지는 것 방지)
    const FName CurrentMap(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));
    if (!LoadInst->SavedMapName.IsNone() && LoadInst->SavedMapName == CurrentMap)
    {
        PlayerPawn->SetActorTransform(LoadInst->PlayerTransform);
    }

    // ANonCharacterBase로 캐스팅
    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(PlayerPawn))
    {
        // 0. 레벨/경험치 복구
        // Note: AttributeSet은 InitAttribute 등으로 초기화되므로, 값을 강제로 덮어써야 함.
        // ASC를 통해 BaseValue를 설정하는 것이 가장 확실함.
        
        // [New] 플레이어 이름 복구
        NonChar->SetPlayerName(LoadInst->PlayerName);

        if (UAbilitySystemComponent* ASC = NonChar->GetAbilitySystemComponent())
        {
            if (const UNonAttributeSet* AS = Cast<UNonAttributeSet>(NonChar->GetAttributeSet()))
            {
                // [Fix] 레벨과 함께 MaxExp 등 스탯도 갱신해줘야 함 (안그러면 경험치 통이 작아서 바로 레벨업함)
                NonChar->SetLevelAndRefreshStats(LoadInst->Level);
                
                // 그 다음 경험치 복구
                ASC->SetNumericAttributeBase(AS->GetExpAttribute(), static_cast<float>(LoadInst->EXP));
                
                // [Fix] 현재 체력/마나 복구 (SetLevelAndRefreshStats가 Max로 채웠을 수 있으니, 저장된 값으로 덮어씀)
                // 단, 저장된 값이 0보다 클 때만 적용 (혹시 모를 오류 방지)
                if (LoadInst->CurrentHP > 0.f)
                {
                    ASC->SetNumericAttributeBase(AS->GetHPAttribute(), LoadInst->CurrentHP);
                }
                if (LoadInst->CurrentMP > 0.f)
                {
                    ASC->SetNumericAttributeBase(AS->GetMPAttribute(), LoadInst->CurrentMP);
                }

            }
        }

        // 2. 스킬 복구
        if (USkillManagerComponent* SkillMgr = NonChar->FindComponentByClass<USkillManagerComponent>())
        {
            SkillMgr->SetJobClass(LoadInst->JobClass);
            SkillMgr->SetSkillPoints(LoadInst->SkillPoints);
            SkillMgr->RestoreSkillLevels(LoadInst->SkillLevels);
        }

        // 3. 인벤토리 복구 (퀵슬롯/장비보다 먼저 해야 함)
        UInventoryComponent* InvenComp = NonChar->FindComponentByClass<UInventoryComponent>();
        if (InvenComp)
        {
            InvenComp->RestoreItemsFromSave(LoadInst->InventoryItems);
            InvenComp->SetGoldFromSave(LoadInst->Gold);
        }

        // [New] 장비 복구 (인벤토리 다음, 퀵슬롯 전)
        if (UEquipmentComponent* Equip = NonChar->FindComponentByClass<UEquipmentComponent>())
        {
            Equip->RestoreEquippedItemsFromSave(LoadInst->EquippedItems);
        }

        // 4. 퀵슬롯 복구
        if (UQuickSlotManager* QM = NonChar->GetQuickSlotManager())
        {
            // 인벤토리 컴포넌트가 필요 (아이템 연결용: 위에서 이미 찾은 변수 재사용)
            UEquipmentComponent* EquipComp = NonChar->FindComponentByClass<UEquipmentComponent>();
            // [Fix] InvenComp 재선언 제거 (위에서 이미 선언됨)
            QM->RestoreQuickSlotsFromSave(LoadInst->QuickSlots, InvenComp, EquipComp);
        }

        // [New] 모든 데이터 복구 후 UI 한 번 강제 갱신 (직업 아이콘 등)
        if (UNonUIManagerComponent* UIMan = NonChar->FindComponentByClass<UNonUIManagerComponent>())
        {
            UIMan->RefreshHUDState();
        }
    }

    OnGameLoaded.Broadcast(true);

}


void USaveGameSubsystem::DeleteSaveGame()
{
    const UNonGameInstance* GI = Cast<UNonGameInstance>(GetGameInstance());
    if (!GI) return;
    const FString TargetSlot = GI->GetCharacterSaveSlotName();

    SetSaveReady(false);
    UGameplayStatics::DeleteGameInSlot(TargetSlot, 0);
    UGameplayStatics::DeleteGameInSlot(GetBackupSlotName(TargetSlot), 0);
}
