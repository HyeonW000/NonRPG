#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSetTypes.h"   // EWeaponStance, FWeaponAnimSet 등
#include "NonAnimInstance.generated.h"

class UAnimSet_Common;
class UAnimSet_Weapon;

/**
 * 프로젝트 공용 AnimInstance
 * - ABP_Non 이 이 클래스를 부모로 사용
 */
UCLASS()
class NON_API UNonAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UNonAnimInstance();

    // ===== 데이터 세트(에디터에서 지정) =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sets")
    TObjectPtr<UAnimSet_Common> CommonSet = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sets")
    TObjectPtr<UAnimSet_Weapon> WeaponSet = nullptr;

    // ===== 상태 값(ABP_Non에서 직접 참조) =====
    UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
    float GroundSpeed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
    float MovementDirection = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
    bool bIsInAir = false;

    UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
    bool bIsAccelerating = false;

    // 가드 중 여부
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Guard", meta = (AllowPrivateAccess = "true"))
    bool bGuarding = false;

    // 가드 방향
    UPROPERTY(BlueprintReadOnly, Category = "Guard", meta = (AllowPrivateAccess = "true"))
    float GuardDirection = 0.f;

    // 무장/스탠스
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "State")
    bool bArmed = false;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "State")
    EWeaponStance WeaponStance = EWeaponStance::Unarmed;

public:
    // ===== 공용 헬퍼 =====
    UFUNCTION(BlueprintPure)
    const FWeaponAnimSet& GetWeaponAnimSet() const;

    UFUNCTION(BlueprintCallable, Category = "Dodge")
    UAnimMontage* GetDodgeByDirIndex(int32 DirIdx) const;

    UFUNCTION(BlueprintCallable, Category = "HitReact")
    UAnimMontage* GetHitReact_Light() const;

    UFUNCTION(BlueprintCallable, Category = "HitReact")
    UAnimMontage* GetHitReact_Heavy() const;

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    UAnimMontage* GetEquipMontage() const;

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    UAnimMontage* GetSheatheMontage() const;

    UFUNCTION(BlueprintCallable, Category = "Common")
    UAnimMontage* GetDeathMontage() const;

    // 외부(캐릭터/어빌리티)에서 가드 on/off 설정용
    UFUNCTION(BlueprintCallable, Category = "Guard")
    void SetGuarding(bool bNewGuarding) { bGuarding = bNewGuarding; }

protected:
    // 틱마다 ABP_Non에서 쓰는 상태값 갱신
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
    UAnimMontage* GetCommonHitReact() const;

    // 내부 계산 함수
    void RefreshMovementStates(float DeltaSeconds);
};
