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
#include "UI/QuickSlotManager.h"

const FString USaveGameSubsystem::DefaultSlotName = TEXT("SaveSlot_01");

void USaveGameSubsystem::SaveGame()
{
    // 로컬 플레이어 폰 찾기
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!PlayerPawn) return;

    UNonSaveGame* SaveInst = Cast<UNonSaveGame>(UGameplayStatics::CreateSaveGameObject(UNonSaveGame::StaticClass()));
    if (!SaveInst) return;

    // 1. 기본 정보 저장
    SaveInst->PlayerTransform = PlayerPawn->GetActorTransform();
    
    // ANonCharacterBase로 캐스팅해서 컴포넌트 접근
    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(PlayerPawn))
    {
        // 0. 레벨/경험치 저장 (AttributeSet에서 가져옴)
        if (const UNonAttributeSet* AS = Cast<UNonAttributeSet>(NonChar->GetAttributeSet()))
        {
            SaveInst->Level = static_cast<int32>(AS->GetLevel());
            SaveInst->EXP = static_cast<int32>(AS->GetExp());
            SaveInst->CurrentHP = AS->GetHP();
            SaveInst->CurrentMP = AS->GetMP();
        }

        // 2. 스킬 저장
        if (USkillManagerComponent* SkillMgr = NonChar->FindComponentByClass<USkillManagerComponent>())
        {
            SaveInst->SkillPoints = SkillMgr->GetSkillPoints();
            SaveInst->JobClass = SkillMgr->GetJobClass();
            SaveInst->SkillLevels = SkillMgr->GetSkillLevelMap();
        }

        // 3. 인벤토리 저장
        if (UInventoryComponent* Inven = NonChar->FindComponentByClass<UInventoryComponent>())
        {
            SaveInst->InventoryItems = Inven->GetItemsForSave();
        }

        // [New] 장비 저장
        if (UEquipmentComponent* Equip = NonChar->FindComponentByClass<UEquipmentComponent>())
        {
            SaveInst->EquippedItems = Equip->GetEquippedItemsForSave();
        }

        // 4. 퀵슬롯 저장
        if (UQuickSlotManager* QM = NonChar->GetQuickSlotManager())
        {
            SaveInst->QuickSlots = QM->GetQuickSlotsForSave();
        }
    }

    // 파일 쓰기
    const bool bSuccess = UGameplayStatics::SaveGameToSlot(SaveInst, DefaultSlotName, 0);
    OnGameSaved.Broadcast(bSuccess);

    UE_LOG(LogTemp, Log, TEXT("[SaveSystem] Game Saved: %s"), bSuccess ? TEXT("Success") : TEXT("Failed"));
}

void USaveGameSubsystem::LoadGame()
{
    if (!UGameplayStatics::DoesSaveGameExist(DefaultSlotName, 0))
    {
        UE_LOG(LogTemp, Warning, TEXT("[SaveSystem] No save file found."));
        return;
    }

    UNonSaveGame* LoadInst = Cast<UNonSaveGame>(UGameplayStatics::LoadGameFromSlot(DefaultSlotName, 0));
    if (!LoadInst) return;

    // 로컬 플레이어 폰 찾기
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!PlayerPawn) return;

    // 1. 위치 복구
    PlayerPawn->SetActorTransform(LoadInst->PlayerTransform);

    // ANonCharacterBase로 캐스팅
    if (ANonCharacterBase* NonChar = Cast<ANonCharacterBase>(PlayerPawn))
    {
        // 0. 레벨/경험치 복구
        // Note: AttributeSet은 InitAttribute 등으로 초기화되므로, 값을 강제로 덮어써야 함.
        // ASC를 통해 BaseValue를 설정하는 것이 가장 확실함.
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
        }

        // [New] 장비 복구 (인벤토리 다음, 퀵슬롯 전)
        if (UEquipmentComponent* Equip = NonChar->FindComponentByClass<UEquipmentComponent>())
        {
            Equip->RestoreEquippedItemsFromSave(LoadInst->EquippedItems);
        }

        // 4. 퀵슬롯 복구
        if (UQuickSlotManager* QM = NonChar->GetQuickSlotManager())
        {
            // 인벤토리 컴포넌트가 필요 (아이템 연결용)
            UEquipmentComponent* EquipComp = NonChar->FindComponentByClass<UEquipmentComponent>();
            QM->RestoreQuickSlotsFromSave(LoadInst->QuickSlots, InvenComp, EquipComp);
        }
    }

    OnGameLoaded.Broadcast(true);
    UE_LOG(LogTemp, Log, TEXT("[SaveSystem] Game Loaded Successfully."));
}


void USaveGameSubsystem::DeleteSaveGame()
{
    if (UGameplayStatics::DoesSaveGameExist(DefaultSlotName, 0))
    {
        bool bSuccess = UGameplayStatics::DeleteGameInSlot(DefaultSlotName, 0);
        UE_LOG(LogTemp, Warning, TEXT("[SaveSystem] DeleteSaveGame: %s"), bSuccess ? TEXT("Success") : TEXT("Failed"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[SaveSystem] DeleteSaveGame: No save file to delete."));
    }
}
