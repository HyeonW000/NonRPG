#include "System/NonSaveGame.h"
#include "Data/ItemIdRedirects.h"

void UNonSaveGame::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);

    if (Ar.IsLoading())
    {
        const int32 LoadedVersion = SaveVersion;
        if (MigrateFromOldVersion())
        {
            UE_LOG(LogTemp, Log, TEXT("[SaveGame] 세이브 형식 변환 %d -> %d"), LoadedVersion, SaveVersion);
        }

        const int32 Changed = MigrateItemIds();
        if (Changed > 0)
        {
            UE_LOG(LogTemp, Log, TEXT("[SaveGame] 옛 아이템 ID %d개를 새 ID로 변환"), Changed);
        }
    }
}

bool UNonSaveGame::MigrateFromOldVersion()
{
    if (SaveVersion >= NonSaveVersion::Current)
    {
        return false;
    }

    // 0 -> 1: 아이템마다 고유 ID 발급 (옛 세이브에는 없음). 골드·맵·기록 칸은 기본값(0, 없음)으로 시작
    if (SaveVersion < 1)
    {
        for (FInventorySaveData& Data : InventoryItems)
        {
            if (!Data.InstanceId.IsValid())
            {
                Data.InstanceId = FGuid::NewGuid();
            }
        }
        for (FEquipmentSaveData& Data : EquippedItems)
        {
            if (!Data.InstanceId.IsValid())
            {
                Data.InstanceId = FGuid::NewGuid();
            }
        }
        SaveVersion = 1;
    }

    return true;
}

int32 UNonSaveGame::MigrateItemIds()
{
    int32 Changed = 0;
    auto Fix = [&Changed](FName& Id)
    {
        if (Id.IsNone())
        {
            return;
        }
        const FName NewId = NonItemIds::Redirect(Id);
        // FName 비교는 대소문자를 무시하므로 문자열로 비교해 대소문자 변경도 반영
        if (!NewId.ToString().Equals(Id.ToString(), ESearchCase::CaseSensitive))
        {
            Id = NewId;
            ++Changed;
        }
    };

    for (FInventorySaveData& Data : InventoryItems)
    {
        Fix(Data.ItemId);
    }
    for (FEquipmentSaveData& Data : EquippedItems)
    {
        Fix(Data.ItemId);
    }
    for (FQuickSlotSaveData& Data : QuickSlots)
    {
        Fix(Data.ItemId);
    }
    return Changed;
}
