#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Inventory/ItemEnums.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "ItemStructs.generated.h"

USTRUCT(BlueprintType)
struct NON_API FEquipmentStatBlock {
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
struct NON_API FConsumableEffect {
  GENERATED_BODY()

  // 쿨그룹/쿨타임
  UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CooldownGroupId;
  UPROPERTY(EditAnywhere, BlueprintReadOnly) float CooldownTime = 0.f;

  // 동일 효과 중첩 허용?
  UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAllowStackingEffect = false;

  // 효과 식별(태그/Id) ? GAS GE/GA 매핑용
  UPROPERTY(EditAnywhere, BlueprintReadOnly)
  FGameplayTag EffectTag; // ex) Data.Item.Effect.HealInstant
  UPROPERTY(EditAnywhere, BlueprintReadOnly)
  float EffectValue = 0.f; // HP+500 등
  UPROPERTY(EditAnywhere, BlueprintReadOnly)
  float EffectDuration = 0.f; // 지속 버프면 > 0
  UPROPERTY(EditAnywhere, BlueprintReadOnly)
  bool bRemovesDebuff = false; // 디버프 해제?
};

USTRUCT(BlueprintType)
struct NON_API FSetBonusEntry {
  GENERATED_BODY()
  UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 PiecesRequired = 2;
  UPROPERTY(EditAnywhere, BlueprintReadOnly)
  FGameplayTag BonusEffectTag; // ex) Data.Item.Set.Bonus.HP
                               // 필요 시 EffectValue/Duration 등 추가
};

USTRUCT(BlueprintType)
struct NON_API FItemRow : public FTableRowBase {
  GENERATED_BODY()

  // ── 공통 (모든 아이템) ──────────────────────────────────────────────────
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common") 
  FName ItemId;
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common") 
  FText Name;
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common", meta = (MultiLine = "true")) 
  FText Description;
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common")
  EItemType ItemType = EItemType::Etc;
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common")
  EItemRarity Rarity = EItemRarity::Common;
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common") 
  TSoftObjectPtr<UTexture2D> Icon;
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common", meta = (ClampMin = "1"))
  int32 MaxStack = 1; // 장비=1, 소비=99 등
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common|Economy") 
  int32 BuyPrice = 0;
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common|Economy") 
  int32 SellPrice = 0;
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common|Economy")
  EBindType BindType = EBindType::None;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common|Economy") 
  bool bNPCShopOnly = false; // NPC 상점에서만 판매되는 아이템 여부
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common") 
  FGameplayTagContainer CustomTags;
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common", meta = (ClampMin = "1"))
  int32 RequiredLevel = 1; // 착용/사용 레벨 제한

  // ── 장비 전용 (Equipment) ──────────────────────────────────────────────────
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "ItemType == EItemType::Equipment", EditConditionHides))
  EEquipmentSlot EquipSlot = EEquipmentSlot::None;
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "ItemType == EItemType::Equipment", EditConditionHides))
  EWeaponType WeaponType = EWeaponType::None;

  // 장착 시 붙일 소켓명 (비워두면 슬롯별 기본 소켓 사용)
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "ItemType == EItemType::Equipment", EditConditionHides)) 
  FName AttachSocket = NAME_None;

  // 장착할 때 스폰할 무기/장비 액터 클래스 (BP_WeaponBase 자식 등)
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "ItemType == EItemType::Equipment", EditConditionHides))
  TSubclassOf<class AActor> EquipActorClass;

  // 액터 스폰 후 껍데기를 즉석에서 갈아끼울 메쉬 (노가다 방지용)
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "ItemType == EItemType::Equipment", EditConditionHides))
  TSoftObjectPtr<USkeletalMesh> SkeletalMesh;
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "ItemType == EItemType::Equipment", EditConditionHides))
  TSoftObjectPtr<UStaticMesh> StaticMesh;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "ItemType == EItemType::Equipment", EditConditionHides)) 
  FEquipmentStatBlock StatBlock;

  // 장착 시 부여할 어빌리티 목록 (예: 무기별 ToggleWeapon 등)
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|GAS", meta = (EditCondition = "ItemType == EItemType::Equipment", EditConditionHides))
  TArray<TSubclassOf<class UGameplayAbility>> GrantedAbilities;

  // 세트 아이템 설정
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|SetBonus", meta = (EditCondition = "ItemType == EItemType::Equipment", EditConditionHides))
  bool bIsSetItem = false;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|SetBonus", meta = (EditCondition = "ItemType == EItemType::Equipment && bIsSetItem", EditConditionHides)) 
  FName SetId; // 세트 식별용 아이디
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|SetBonus", meta = (EditCondition = "ItemType == EItemType::Equipment && bIsSetItem", EditConditionHides)) 
  TArray<FSetBonusEntry> SetBonuses;

  // ── 소비품 전용 (Consumable) ──────────────────────────────────────────────────
  
  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable", meta = (EditCondition = "ItemType == EItemType::Consumable", EditConditionHides)) 
  FConsumableEffect Consumable;
};
