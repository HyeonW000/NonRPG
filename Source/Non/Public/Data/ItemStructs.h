#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Inventory/ItemEnums.h"
#include "ItemStructs.generated.h"

USTRUCT(BlueprintType)
struct NON_API FEquipmentStatBlock
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) float AttackPower = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float DefensePower = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MagicPower = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MagicResist = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MoveSpeedBonus = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float CritChance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float CritDamage = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float CooldownReduction = 0.f;
};

USTRUCT(BlueprintType)
struct NON_API FConsumableEffect
{
    GENERATED_BODY()

    // 쿨그룹/쿨타임
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CooldownGroupId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float CooldownTime = 0.f;

    // 동일 효과 중첩 허용?
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAllowStackingEffect = false;

    // 효과 식별(태그/Id) ? GAS GE/GA 매핑용
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag EffectTag; // ex) Data.Item.Effect.HealInstant
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float EffectValue = 0.f;     // HP+500 등
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float EffectDuration = 0.f;  // 지속 버프면 > 0
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bRemovesDebuff = false; // 디버프 해제?
};

USTRUCT(BlueprintType)
struct NON_API FSetBonusEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 PiecesRequired = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag BonusEffectTag; // ex) Data.Item.Set.Bonus.HP
    // 필요 시 EffectValue/Duration 등 추가
};

USTRUCT(BlueprintType)
struct NON_API FItemRow : public FTableRowBase
{
    GENERATED_BODY()

    // 공통
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ItemId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Name;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EItemType ItemType = EItemType::Etc;

    // 장착 시 붙일 소켓명 (비워두면 슬롯별 기본 소켓 또는 VisualSlots에서 지정된 소켓을 사용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName AttachSocket = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EItemRarity Rarity = EItemRarity::Common;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MaxStack = 1; // 장비=1, 소비=99 등
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 BuyPrice = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 SellPrice = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float DropRate = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EBindType BindType = EBindType::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTagContainer CustomTags;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 RequiredLevel = 1; // 착용/사용 레벨 제한

    // 장비 전용
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EEquipmentSlot EquipSlot = EEquipmentSlot::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EWeaponType WeaponType = EWeaponType::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FEquipmentStatBlock StatBlock;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 EnhancementLevel = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SetId; // 세트 식별
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FSetBonusEntry> SetBonuses;

    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<USkeletalMesh> SkeletalMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UStaticMesh>   StaticMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UNiagaraSystem> EquipVFX;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<USoundBase>    EquipSFX;

    // 소비 전용
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FConsumableEffect Consumable;

    // 기타 전용
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bQuestItem = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bTradable = true;   // 거래 가능 여부
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bNPCShopOnly = false;

    // [New] 장착 시 부여할 어빌리티 목록 (예: 무기별 ToggleWeapon 등)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
    TArray<TSubclassOf<class UGameplayAbility>> GrantedAbilities;
};
