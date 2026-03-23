#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class NON_API AWeaponBase : public AActor {
  GENERATED_BODY()

public:
  AWeaponBase();

protected:
  virtual void BeginPlay() override;

public:
  virtual void Tick(float DeltaTime) override;

  // 이 무기의 최상위 루트
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
  USceneComponent *Root;

  // 스켈레탈 메시 외형 (선택)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
  USkeletalMeshComponent *WeaponSkeletalMesh;

  // 스태틱 메시 외형 (선택)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
  UStaticMeshComponent *WeaponStaticMesh;

  // 대망의 트레일 시작점 (칼등/손잡이 부근)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Trail")
  USceneComponent *TrailStart;

  // 대망의 트레일 끝점 (칼끝)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Trail")
  USceneComponent *TrailEnd;
};
