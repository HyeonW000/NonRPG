#pragma once

#include "CoreMinimal.h"

/**
 * 아이템 ID 변경 기록 (옛 ID -> 새 ID).
 * DT_ItemData 행 이름을 바꿀 때 여기에 한 줄 추가하면, 옛 세이브(인벤토리·장비·퀵슬롯)를 불러올 때 새 ID로 바뀐다.
 * 규칙: Naming_Conventions.md 4장 "아이템 ID". 한 번 넣은 줄은 지우지 않는다 (오래된 세이브 호환).
 */
namespace NonItemIds
{
    /** 옛 ID면 새 ID를, 아니면 그대로 돌려준다 */
    inline FName Redirect(FName ItemId)
    {
        static const TMap<FName, FName> Redirects = {
            // 2026-11-26 아이템 ID 규칙 적용
            { FName(TEXT("Greatsword_Bronze")), FName(TEXT("GreatSword_Bronze")) },
            { FName(TEXT("WoodShield")),        FName(TEXT("Shield_Wood")) },
            { FName(TEXT("Glove")),             FName(TEXT("Hands_Leather")) },
            { FName(TEXT("Staff")),             FName(TEXT("Staff_Wood")) },
            { FName(TEXT("Potion")),            FName(TEXT("Potion_HP_Small")) },
            { FName(TEXT("Potion1")),           FName(TEXT("Potion_HP_Medium")) },
            { FName(TEXT("Helm")),              FName(TEXT("Head_Iron")) },
        };

        // FName 비교는 대소문자를 구분하지 않으므로 Greatsword_Bronze == GreatSword_Bronze.
        // 대소문자만 다른 항목은 결과가 같은 이름이라 문제없다.
        if (const FName* NewId = Redirects.Find(ItemId))
        {
            return *NewId;
        }
        return ItemId;
    }
}
