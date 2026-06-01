#include "UI/Inventory/ItemToolTipWidget.h"
#include "Inventory/InventoryItem.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h" // ◀ [New] RichText UMG 연동 헤더 포함!
#include "Components/Border.h"
#include "Components/Image.h"
#include "Equipment/EquipmentComponent.h" // ◀ [New] 실시간 장착 세트 개수 추적용 헤더 포함!

void UItemToolTipWidget::SetItemData(UInventoryItem* InItem)
{
    if (!InItem) return;

    // ── 1. 아이템 이름 셋팅 ─────────────────────────────
    if (TxtItemName)
    {
        // 강화 수치가 있으면 이름 뒤에 "+3" 형태로 세련되게 붙여줍니다.
        if (InItem->EnhancementLevel > 0)
        {
            FString EncStr = FString::Printf(TEXT("%s (+%d)"), *InItem->CachedRow.Name.ToString(), InItem->EnhancementLevel);
            TxtItemName->SetText(FText::FromString(EncStr));
        }
        else
        {
            TxtItemName->SetText(InItem->CachedRow.Name);
        }
    }





    // ── 4. 가격 한글 및 세련된 쉼표 표시 포맷팅 ─────────────────────
    if (TxtPrice)
    {
        // 3자리마다 자동으로 쉼표가 찍히는 멋진 가격 포맷 (예: "15,000 Gold")
        FString PriceStr = FString::Printf(TEXT("%s Gold"), *FText::AsNumber(InItem->CachedRow.BuyPrice).ToString());
        TxtPrice->SetText(FText::FromString(PriceStr));
    }

    // ── [New] 5. 아이콘 이미지 셋팅 ──────────────────────
    if (ImgIcon)
    {
        UTexture2D* IconTex = InItem->GetIcon();
        if (IconTex)
        {
            ImgIcon->SetBrushFromTexture(IconTex);
            ImgIcon->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            ImgIcon->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // ── [New] 5.5 장비 세부 능력치(Main Stats) 자동 가공 및 실시간 비교 주입 ──────────
    if (TxtMainStats)
    {
        if (InItem->CachedRow.ItemType == EItemType::Equipment)
        {
            FString StatStr;
            const FEquipmentStatBlock& SB = InItem->CachedRow.StatBlock;

            // 실시간 동일 부위 장착 장비 스탯 비교
            UInventoryItem* EquippedItem = nullptr;
            if (APawn* PlayerPawn = GetOwningPlayerPawn())
            {
                if (UEquipmentComponent* EquipComp = PlayerPawn->FindComponentByClass<UEquipmentComponent>())
                {
                    EEquipmentSlot EquipSlot = InItem->GetEquipSlot();
                    if (EquipSlot != EEquipmentSlot::None)
                    {
                        EquippedItem = EquipComp->GetEquippedItemBySlot(EquipSlot);
                    }
                }
            }

            // 비교 연산을 돕기 위한 람다식 구성 (RichText 태그 주입)
            auto FormatStatLine = [&](const FString& StatLabel, float NewVal, float OldVal, bool bIsPercentage = false) -> FString {
                // 1. 현재 보는 가방 아이템에 해당 효과(NewVal)가 아예 없으면 툴팁에 표시하지 않습니다.
                if (NewVal <= 0.f)
                {
                    return TEXT("");
                }

                FString Line = FString::Printf(TEXT("▶ %s +%.0f%s"), *StatLabel, NewVal, bIsPercentage ? TEXT("%") : TEXT(""));

                // 2. 장착 중인 아이템이 존재할 경우 비교를 처리합니다.
                if (EquippedItem && EquippedItem != InItem)
                {
                    if (OldVal > 0.f)
                    {
                        float Delta = NewVal - OldVal;
                        if (Delta > 0.f)
                        {
                            // 상승치 부분만 up 태그로 감싸서 예쁜 컬러 부여
                            Line += FString::Printf(TEXT(" <up>( + %.0f%s)</>"), Delta, bIsPercentage ? TEXT("%") : TEXT(""));
                        }
                        else if (Delta < 0.f)
                        {
                            // 하강치 부분만 down 태그로 감싸서 예쁜 컬러 부여
                            Line += FString::Printf(TEXT(" <down>( - %.0f%s)</>"), FMath::Abs(Delta), bIsPercentage ? TEXT("%") : TEXT(""));
                        }
                    }
                    else
                    {
                        // 장착 중인 장비에는 없는 신규 옵션이므로 (+) 로만 깔끔하게 시각 피드백을 제공합니다.
                        Line += TEXT(" <up>( + )</>");
                    }
                }
                Line += TEXT("\n");
                return Line;
            };

            const FEquipmentStatBlock& EquippedSB = EquippedItem ? EquippedItem->CachedRow.StatBlock : FEquipmentStatBlock();

            StatStr += FormatStatLine(TEXT("물리 공격력"), SB.AttackPower, EquippedSB.AttackPower);
            StatStr += FormatStatLine(TEXT("물리 방어력"), SB.DefensePower, EquippedSB.DefensePower);
            StatStr += FormatStatLine(TEXT("마법 공격력"), SB.MagicPower, EquippedSB.MagicPower);
            StatStr += FormatStatLine(TEXT("마법 저항력"), SB.MagicResist, EquippedSB.MagicResist);
            StatStr += FormatStatLine(TEXT("치명타 확률"), SB.CritChance, EquippedSB.CritChance, true);
            StatStr += FormatStatLine(TEXT("치명타 피해"), SB.CritDamage, EquippedSB.CritDamage, true);
            StatStr += FormatStatLine(TEXT("이동 속도"), SB.MoveSpeedBonus, EquippedSB.MoveSpeedBonus);
            StatStr += FormatStatLine(TEXT("쿨다운 감소"), SB.CooldownReduction, EquippedSB.CooldownReduction, true);

            if (!StatStr.IsEmpty())
            {
                // 맨 마지막 줄의 불필요한 개행(\n)을 제거하여 텍스트 여백 오버헤드 원천 제거!
                StatStr.RemoveFromEnd(TEXT("\n"));
                TxtMainStats->SetText(FText::FromString(StatStr));
                TxtMainStats->SetVisibility(ESlateVisibility::Visible);
            }
            else
            {
                TxtMainStats->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
        else
        {
            // 장비가 아니면 스탯 칸을 닫아 툴팁 크기를 콤팩트하게 유지합니다.
            TxtMainStats->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // ── [New] 6. 세트 아이템 정보 가공 및 텍스트 셋팅 ─────────────────
    if (InItem->CachedRow.bIsSetItem)
    {
        // ── [UX 혁신 3단계] 플레이어 캐릭터로부터 현재 장착한 동일 세트 장비 개수 실시간 카운팅!
        int32 EquippedSetCount = 0;
        if (APawn* PlayerPawn = GetOwningPlayerPawn())
        {
            if (UEquipmentComponent* EquipComp = PlayerPawn->FindComponentByClass<UEquipmentComponent>())
            {
                for (const auto& Pair : EquipComp->Equipped)
                {
                    UInventoryItem* EquippedItem = Pair.Value;
                    if (EquippedItem && EquippedItem->CachedRow.bIsSetItem && EquippedItem->CachedRow.SetId == InItem->CachedRow.SetId)
                    {
                        EquippedSetCount++;
                    }
                }
            }
        }

        // 최대 세트 아이템 수(N) 실시간 연산 (기입된 세트 효과 중 최댓값 추적)
        int32 MaxSetPieces = 0;
        for (const FSetBonusEntry& Entry : InItem->CachedRow.SetBonuses)
        {
            if (Entry.PiecesRequired > MaxSetPieces)
            {
                MaxSetPieces = Entry.PiecesRequired;
            }
        }

        // 세트 전용 식별 명칭 텍스트블록 연동 세팅 (장착중인 카드일 경우에만 분수비 (X / N) 노출!)
        if (TxtSetName)
        {
            FString SetNameStr = FString::Printf(TEXT("세트: [%s]"), *InItem->CachedRow.SetId.ToString());
            if (bIsComparePanel)
            {
                SetNameStr += FString::Printf(TEXT(" (%d / %d)"), EquippedSetCount, MaxSetPieces);
            }
            TxtSetName->SetText(FText::FromString(SetNameStr));
            TxtSetName->SetVisibility(ESlateVisibility::Visible);
            
            // 세트 이름 글씨는 명품 장비다운 화려하고 영롱한 맑은 황금색 계열로 칠해 감성 극대화!
            TxtSetName->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.35f, 1.0f)));
        }

        // 영문 태그 한글 번역 사전
        auto TranslateTagToKorean = [](const FString& TagName) -> FString {
            if (TagName.Contains(TEXT("AttackPower")) || TagName.Contains(TEXT("Attack"))) return TEXT("공격력 +10");
            if (TagName.Contains(TEXT("DefensePower")) || TagName.Contains(TEXT("Defense"))) return TEXT("방어력 +10");
            if (TagName.Contains(TEXT("MagicPower")) || TagName.Contains(TEXT("Magic"))) return TEXT("주문력 +15");
            if (TagName.Contains(TEXT("MagicResist"))) return TEXT("마법 저항력 +10");
            if (TagName.Contains(TEXT("CritChance")) || TagName.Contains(TEXT("Crit"))) return TEXT("치명타 확률 +5%");
            if (TagName.Contains(TEXT("HP"))) return TEXT("최대 체력 +500");
            if (TagName.Contains(TEXT("Explosion"))) return TEXT("공격 시 5% 확률로 폭발 발생");
            
            FString Fallback = TagName;
            Fallback.RemoveFromStart(TEXT("Data.Item.Set.Bonus."));
            return Fallback;
        };

        // 동일 필요 개수별 효과 그룹화
        TMap<int32, TArray<FString>> GroupedBonuses;
        for (const FSetBonusEntry& Entry : InItem->CachedRow.SetBonuses)
        {
            FString RawTagName = Entry.BonusEffectTag.GetTagName().ToString();
            FString KoreanEffect = TranslateTagToKorean(RawTagName);
            
            GroupedBonuses.FindOrAdd(Entry.PiecesRequired).Add(KoreanEffect);
        }

        // 3, 5 세트 위젯에 매칭되는 Pieces를 찾고 텍스트 채우기 및 색상 토글
        auto SetupSetEffectWidget = [&](UTextBlock* Widget, int32 Pieces) {
            if (Widget)
            {
                if (GroupedBonuses.Contains(Pieces))
                {
                    const TArray<FString>& Effects = GroupedBonuses[Pieces];
                    FString JoinedEffects = FString::Join(Effects, TEXT(", "));
                    FString SetStr = FString::Printf(TEXT("▶ %d세트 효과: %s"), Pieces, *JoinedEffects);
                    
                    Widget->SetText(FText::FromString(SetStr));
                    Widget->SetVisibility(ESlateVisibility::Visible);
                    
                    // 장착 개수에 따라 활성 / 비활성 색상 지정 (에디터 디테일 창에서 변경 가능)
                    const bool bIsEffectActive = (EquippedSetCount >= Pieces);
                    Widget->SetColorAndOpacity(FSlateColor(bIsEffectActive ? ActiveSetColor : InactiveSetColor));
                }
                else
                {
                    Widget->SetVisibility(ESlateVisibility::Collapsed);
                }
            }
        };

        SetupSetEffectWidget(TxtSet2Effect, 2);
        SetupSetEffectWidget(TxtSet3Effect, 3);
        SetupSetEffectWidget(TxtSet4Effect, 4);
        SetupSetEffectWidget(TxtSet5Effect, 5);
        SetupSetEffectWidget(TxtSet6Effect, 6);
    }
    else
    {
        // ── [UX 혁신] 장비 아이템인데 세트 템이 아닌 경우에만 '세트 효과 없음' 을 띄우도록 가공!
        if (InItem->CachedRow.ItemType == EItemType::Equipment)
        {
            if (TxtSetName)
            {
                TxtSetName->SetText(FText::FromString(TEXT("세트 효과 없음")));
                TxtSetName->SetVisibility(ESlateVisibility::Visible);
                // 세트 효과가 없으므로 차분하고 얌전한 어두운 은회색 단색으로 처리
                TxtSetName->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
            }
        }
        else
        {
            // 장비 자체가 아닌 일반 아이템(포션 등)은 깔끔하게 완전히 가려 크기 최소화!
            if (TxtSetName) TxtSetName->SetVisibility(ESlateVisibility::Collapsed);
        }

        // 개별 가변 세트 라인 위젯들은 모두 Collapsed로 접음
        if (TxtSet2Effect) TxtSet2Effect->SetVisibility(ESlateVisibility::Collapsed);
        if (TxtSet3Effect) TxtSet3Effect->SetVisibility(ESlateVisibility::Collapsed);
        if (TxtSet4Effect) TxtSet4Effect->SetVisibility(ESlateVisibility::Collapsed);
        if (TxtSet5Effect) TxtSet5Effect->SetVisibility(ESlateVisibility::Collapsed);
        if (TxtSet6Effect) TxtSet6Effect->SetVisibility(ESlateVisibility::Collapsed);
    }

    // ── 7. 아이템 등급에 따른 전용 컬러 테이블 연산 ────────────────────
    FLinearColor RarityColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f); // 기본 회색 (Common)
    FText RarityText = FText::FromString(TEXT("일반"));

    // 디자이너가 에디터에서 설정한 RarityColors가 있으면 그것을 최우선적으로 상속받아 칠합니다
    if (RarityColors.Contains(InItem->CachedRow.Rarity))
    {
        RarityColor = RarityColors[InItem->CachedRow.Rarity];
    }
    else
    {
        switch (InItem->CachedRow.Rarity)
        {
            case EItemRarity::Uncommon:  RarityColor = FLinearColor(0.12f, 0.77f, 0.12f, 1.0f); break; // 연초록색
            case EItemRarity::Rare:      RarityColor = FLinearColor(0.0f, 0.47f, 0.95f, 1.0f);  break; // 푸른색 (파랑)
            case EItemRarity::Epic:      RarityColor = FLinearColor(0.63f, 0.13f, 0.94f, 1.0f); break; // 보라색
            case EItemRarity::Legendary: RarityColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);    break; // 황금빛 주황색
            case EItemRarity::Mythic:    RarityColor = FLinearColor(0.85f, 0.08f, 0.23f, 1.0f); break; // 강렬한 루비 적색
            default: break;
        }
    }

    // 등급 텍스트 매칭은 안정적으로 기존 체계 유지
    switch (InItem->CachedRow.Rarity)
    {
        case EItemRarity::Uncommon:  RarityText = FText::FromString(TEXT("고급")); break;
        case EItemRarity::Rare:      RarityText = FText::FromString(TEXT("희귀")); break;
        case EItemRarity::Epic:      RarityText = FText::FromString(TEXT("영웅")); break;
        case EItemRarity::Legendary: RarityText = FText::FromString(TEXT("전설")); break;
        case EItemRarity::Mythic:    RarityText = FText::FromString(TEXT("신화")); break;
        default: break;
    }

    // 등급 텍스트 주입 - (등급 / 종류) 형태로 세련되게 조립하여 출력합니다!
    if (TxtItemRarity)
    {
        FString TypeStr;
        if (InItem->CachedRow.ItemType == EItemType::Equipment)
        {
            // 1. 만약 무기 종류가 지정되어 있다면 무기 상세 종류로 변환
            if (InItem->CachedRow.WeaponType != EWeaponType::None)
            {
                switch (InItem->CachedRow.WeaponType)
                {
                    case EWeaponType::OneHanded: TypeStr = TEXT("한손 검"); break;
                    case EWeaponType::TwoHanded: TypeStr = TEXT("양손 검"); break;
                    case EWeaponType::Staff:     TypeStr = TEXT("스태프"); break;
                    case EWeaponType::WeaponSub:  TypeStr = TEXT("보조 무기"); break;
                    default:                     TypeStr = TEXT("무기"); break;
                }
            }
            // 2. 무기가 아닌 방어구/장신구라면 장착 슬롯을 기준으로 한글 종류 가공
            else
            {
                switch (InItem->GetEquipSlot())
                {
                    case EEquipmentSlot::Head:       TypeStr = TEXT("머리 방어구"); break;
                    case EEquipmentSlot::Chest:      TypeStr = TEXT("가슴 방어구"); break;
                    case EEquipmentSlot::Legs:       TypeStr = TEXT("다리 방어구"); break;
                    case EEquipmentSlot::Hands:      TypeStr = TEXT("장갑"); break;
                    case EEquipmentSlot::Feet:       TypeStr = TEXT("신발"); break;
                    case EEquipmentSlot::WeaponSub:  TypeStr = TEXT("보조 장비"); break;
                    case EEquipmentSlot::Accessory1: 
                    case EEquipmentSlot::Accessory2: TypeStr = TEXT("장신구"); break;
                    default:                         TypeStr = TEXT("장비"); break;
                }
            }
        }
        else
        {
            switch (InItem->CachedRow.ItemType)
            {
                case EItemType::Consumable:  TypeStr = TEXT("소비품"); break;
                case EItemType::Material:    TypeStr = TEXT("재료"); break;
                case EItemType::Quest:       TypeStr = TEXT("퀘스트"); break;
                case EItemType::Cosmetic:    TypeStr = TEXT("꾸미기"); break;
                default:                     TypeStr = TEXT("기타"); break;
            }
        }

        FString CombinedStr = FString::Printf(TEXT("(%s / %s)"), *RarityText.ToString(), *TypeStr);
        TxtItemRarity->SetText(FText::FromString(CombinedStr));
        
        // ◀ [New UI UX] 등급/종류 텍스트의 색상을 쨍하게 칠하지 않고 차분한 은회색 단색으로 톤다운시켜 명품 가독성을 확보합니다!
        TxtItemRarity->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f)));
    }

    // ── [New] 7.5 독립된 장착 중 텍스트 블록 실시간 표기 및 제어 ──────────────────
    if (TxtEquipped)
    {
        if (bIsComparePanel)
        {
            TxtEquipped->SetText(FText::FromString(TEXT("[장착 중]")));
            TxtEquipped->SetVisibility(ESlateVisibility::Visible);
            // 장착 표시 텍스트는 영롱하고 가시성이 뛰어난 황금빛 색상으로 칠해서 명품 UI 느낌 유도!
            TxtEquipped->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.75f, 0.15f, 1.0f)));
        }
        else
        {
            TxtEquipped->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // 등급 전용 색상 틴트로 이름 글씨 칠하기 (에픽은 보라색 글씨 등)
    if (TxtItemName)
    {
        TxtItemName->SetColorAndOpacity(FSlateColor(RarityColor));
    }

    // [New] 아이콘 주변의 테두리 보더에 선명하고 확실한 등급 전용 포인트 컬러 주입
    if (BorderIconFrame)
    {
        BorderIconFrame->SetBrushColor(RarityColor);
    }

    // [New] 툴팁 가장 밑바닥에 디자이너가 깔아둘 배경 보더가 존재하면 디자이너 설정 알파값을 상속하여 등급 아우라 적용
    if (BorderBackground)
    {
        FLinearColor BgColor = RarityColor;
        BgColor.A = BorderBackground->GetBrushColor().A; // 디자이너가 설정한 원래 알파값 완벽 보존
        BorderBackground->SetBrushColor(BgColor);
    }

    // 등급 전용 색상으로 툴팁 테두리선(BorderOutline) 아름답게 칠하기
    if (BorderOutline)
    {
        // ◀ [New UI UX] 배경 그라데이션과 이름의 색상이 같아 글자가 묻히는 가독성 저하를 해결하기 위해,
        // 배경 보더의 투명도를 0.15f로 대폭 낮추어 은은하게 뿜어져 나오는 영롱한 안개빛 아우라로 연출합니다!
        FLinearColor GlowAuraColor = RarityColor;
        GlowAuraColor.A = 0.15f;
        BorderOutline->SetBrushColor(GlowAuraColor);
    }

    // ── [New UX] 8. 장착 중인 비교 장비 2중 팝업 패널 제어 ──────────────────
    if (!bIsComparePanel && CompareToolTipWidget)
    {
        UInventoryItem* EquippedCompareItem = nullptr;
        if (InItem->CachedRow.ItemType == EItemType::Equipment)
        {
            if (APawn* PlayerPawn = GetOwningPlayerPawn())
            {
                if (UEquipmentComponent* EquipComp = PlayerPawn->FindComponentByClass<UEquipmentComponent>())
                {
                    EEquipmentSlot EquipSlot = InItem->GetEquipSlot();
                    if (EquipSlot != EEquipmentSlot::None)
                    {
                        EquippedCompareItem = EquipComp->GetEquippedItemBySlot(EquipSlot);
                    }
                }
            }
        }

        if (EquippedCompareItem && EquippedCompareItem != InItem)
        {
            CompareToolTipWidget->bIsComparePanel = true;
            CompareToolTipWidget->SetItemData(EquippedCompareItem);
            CompareToolTipWidget->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            CompareToolTipWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    else if (bIsComparePanel && CompareToolTipWidget)
    {
        // [New UX] 장착 슬롯 호버 등으로 이미 비교 패널 자체로 사용 중인 경우에는 2중 비교가 뜨지 않도록 강제 숨김 처리
        CompareToolTipWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

