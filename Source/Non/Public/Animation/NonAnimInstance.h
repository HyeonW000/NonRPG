#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSetTypes.h"     // EWeaponStance, F*Set
#include "Animation/AnimSet_Common.h"    // UAnimSet_Common
#include "Animation/AnimSet_Weapon.h"   // UAnimSet_Weapon
#include "NonAnimInstance.generated.h"

class ANonCharacterBase;

/**
 * 캐릭터/GA에서 애셋을 직접 들고있지 않고
 * AnimBP(AnimInstance) + DataAsset에서 모든 애니메이션 참조를 관리하는 허브
 */
UCLASS()
class NON_API UNonAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    /** ===== 상태 동기화용 ===== */
    UPROPERTY(BlueprintReadOnly, Category = "Owner")
    TObjectPtr<ANonCharacterBase> OwnerChar = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    float GroundSpeed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    float MovementDirection = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bIsInAir = false;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bIsAccelerating = false;

    UPROPERTY(BlueprintReadOnly, Category = "Weapon")
    bool bArmed = false;

    UPROPERTY(BlueprintReadOnly, Category = "Weapon")
    EWeaponStance WeaponStance = EWeaponStance::Unarmed;

    UPROPERTY(BlueprintReadOnly, Category = "Weapon")
    bool bUseFullBodyDrawSheathe = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard")
    bool bGuarding = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard")
    float GuardDirection = 0.f; // -180 .. +180

    /** ===== 데이터에셋(애님 세트) ===== */
    // 클래스 공통(IdleRun, HitReact, Death 등)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AnimSets", meta = (DisplayName = "Common Set"))
    TObjectPtr<UAnimSet_Common> ClassSet = nullptr; // 타입은 Common, 표기는 Common Set

    // 무기 스탠스별(Equip/Draw/Dodge/Guard/AttackCombo 등)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AnimSets")
    TObjectPtr<UAnimSet_Weapon> WeaponSet = nullptr;

public:
    /** ===== 안전한 Getter ===== */
    UFUNCTION(BlueprintCallable, Category = "AnimSets|Class")
    UBlendSpace* GetIdleRunBlendSpace() const;

    UFUNCTION(BlueprintCallable, Category = "AnimSets|Class")
    UAnimMontage* GetDeathMontage() const;

    UFUNCTION(BlueprintCallable, Category = "AnimSets|Class")
    UAnimMontage* GetHitReactMontage_Light() const;

    UFUNCTION(BlueprintCallable, Category = "AnimSets|Class")
    UAnimMontage* GetHitReactMontage_Heavy() const;

    /** 무기 스탠스에 맞는 세트 꺼내기 */
    UFUNCTION(BlueprintCallable, Category = "AnimSets|Weapon")
    const FWeaponAnimSet& GetWeaponAnimSet() const;

    /** 공용: 장비/해제/구르기/가드 등 */
    UFUNCTION(BlueprintCallable, Category = "AnimSets|Weapon")
    UAnimMontage* GetEquipMontage() const;

    UFUNCTION(BlueprintCallable, Category = "AnimSets|Weapon")
    UAnimMontage* GetSheatheMontage() const;

    UFUNCTION(BlueprintCallable, Category = "AnimSets|Weapon")
    UAnimMontage* GetDodgeMontage() const;

    UFUNCTION(BlueprintCallable, Category = "AnimSets|Weapon")
    UAnimMontage* GetGuardMontage() const;

    /** 콤보: 섹션 단일 몽타주 방식이라면 Combo1/2/3 중 원하는 것 리턴 */
    UFUNCTION(BlueprintCallable, Category = "AnimSets|Weapon")
    UAnimMontage* GetComboMontage(int32 ComboIndex /*1-based: 1,2,3*/) const;
};
