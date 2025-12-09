#include "System/SaveGameSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Character/NonCharacterBase.h"
#include "Skill/SkillManagerComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryItem.h"

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
        // 2. 스킬 복구
        if (USkillManagerComponent* SkillMgr = NonChar->FindComponentByClass<USkillManagerComponent>())
        {
            SkillMgr->SetJobClass(LoadInst->JobClass);
            SkillMgr->SetSkillPoints(LoadInst->SkillPoints);
            SkillMgr->RestoreSkillLevels(LoadInst->SkillLevels);
        }

        // 3. 인벤토리 복구
        if (UInventoryComponent* Inven = NonChar->FindComponentByClass<UInventoryComponent>())
        {
            Inven->RestoreItemsFromSave(LoadInst->InventoryItems);
        }
    }

    OnGameLoaded.Broadcast(true);
    UE_LOG(LogTemp, Log, TEXT("[SaveSystem] Game Loaded Successfully."));
}
