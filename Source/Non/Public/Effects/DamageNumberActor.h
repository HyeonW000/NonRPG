#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Curves/CurveFloat.h"
#include "UI/NonDamageTypes.h"
#include "DamageNumberActor.generated.h"

class UWidgetComponent;
class UDamageNumberWidget;

UCLASS()
class NON_API ADamageNumberActor : public AActor
{
    GENERATED_BODY()
public:
    ADamageNumberActor();

    // 스폰 직후 숫자 설정
    void Init(float InDamage, ENonDamageNumberCategory InCategory, int32 InFontSize = 28);

    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void InitWithFlags(float InAmount, bool bInCritical);

    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void InitAsLabel(const FText& InText, ENonDamageNumberCategory InCategory, int32 InFontSize = 28);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber")
    bool bIsDodge = false;

    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void SetupNumeric(float InValue, bool bCritical, const FLinearColor& InColor);
    // "DODGE" 라벨 세팅
    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void SetupAsDodge();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, Category = "DamageNumber")
    TObjectPtr<UWidgetComponent> WidgetComp;

    // 연출 파라미터 (위젯 애니메이션 시간보다 길어야 합니다)
    UPROPERTY(EditDefaultsOnly, Category = "DamageNumber|FX")
    float LifeTime = 1.5f;

    float Age = 0.f;

private:
    float PendingAmount = 0.f;
    int32 PendingFontSize = 28;
    ENonDamageNumberCategory PendingType = ENonDamageNumberCategory::Normal;

    bool bPendingLabel = false;
    FText PendingLabel;
};
