#include "Inventory/ItemBase.h"
#include "Engine/DataTable.h"

bool UItemBase::LoadFromDataTable()
{
    if (!ItemDataTable || ItemId.IsNone()) return false;
    if (const FItemRow* Row = ItemDataTable->FindRow<FItemRow>(ItemId, TEXT("UItemBase::LoadFromDataTable")))
    {
        ItemData = *Row;
        return true;
    }
    return false;
}
