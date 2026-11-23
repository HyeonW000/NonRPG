#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JustDodgeAfterimage.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMeshComponent;

/**
 * 저스트 회피 잔상 (설계서 5장 16).
 * 생성 순간 대상 캐릭터의 모든 메시(몸/머리/장비/무기) 자세를 복사해 반투명 머티리얼로 남기고,
 * Lifetime 동안 투명해지다가 사라진다. 각 화면에서 로컬로만 생성 (복제 안 함).
 */
UCLASS(Blueprintable)
class NON_API AJustDodgeAfterimage : public AActor
{
    GENERATED_BODY()

public:
    AJustDodgeAfterimage();

    /** Source의 메시들을 복사해 잔상을 만든다. 스폰 직후 호출 */
    void InitFromActor(AActor* Source, UMaterialInterface* InMaterial, const FLinearColor& InColor, float InLifetime);

    /** 머티리얼 파라미터 이름 (M_Afterimage 기준) */
    UPROPERTY(EditDefaultsOnly, Category = "Afterimage")
    FName OpacityParamName = TEXT("Opacity");

    UPROPERTY(EditDefaultsOnly, Category = "Afterimage")
    FName ColorParamName = TEXT("Color");

    /** 시작 투명도 */
    UPROPERTY(EditDefaultsOnly, Category = "Afterimage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float StartOpacity = 0.8f;

protected:
    virtual void Tick(float DeltaSeconds) override;

private:
    void AddMeshCopy(UMeshComponent* SourceComp);

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> MID;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMeshComponent>> CopiedMeshes;

    float Lifetime = 0.4f;
    float Age = 0.f;
};
