#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Curves/CurveFloat.h"
#include "DamageNumberActor.generated.h"

class UWidgetComponent;
class UDamageNumberWidget;

UCLASS()
class NON_API ADamageNumberActor : public AActor
{
    GENERATED_BODY()
public:
    ADamageNumberActor();

    // 스폰 직후 숫자/색/크기 설정
    void Init(float InDamage, const FLinearColor& InColor, int32 InFontSize = 28);

    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void InitWithFlags(float InAmount, bool bInCritical);

    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void InitAsLabel(const FText& InText, const FLinearColor& InColor = FLinearColor::White, int32 InFontSize = 28);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber")
    bool bIsDodge = false;

    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void SetupNumeric(float InValue, bool bCritical, const FLinearColor& InColor);

    UFUNCTION(BlueprintCallable, Category = "DamageNumber")
    void SetupAsDodge(); // ← "DODGE" 라벨 세팅

    // === 변경점: BP에서 바로 바꿀 수 있는 기본값들 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Style")
    FLinearColor NormalDamageColor = FLinearColor(0.f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Style")
    FLinearColor CriticalDamageColor = FLinearColor(1.f, 0.9f, 0.2f); // 크리티컬 → 노랑/골드

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Style")
    FLinearColor DodgeTextColor = FLinearColor::White;                // DODGE → 흰색

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Style")
    int32 NumberFontSize = 28;                                        // 숫자 크기

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Style")
    int32 DodgeFontSize = 28;                                         // DODGE 라벨 크기

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, Category = "DamageNumber")
    TObjectPtr<UWidgetComponent> WidgetComp;

    // 연출 파라미터
    UPROPERTY(EditDefaultsOnly, Category = "DamageNumber|FX")
    float LifeTime = 1.2f;

    UPROPERTY(EditDefaultsOnly, Category = "DamageNumber|FX")
    float RiseSpeed = 60.f;   // 초당 상승 거리

    UPROPERTY(EditDefaultsOnly, Category = "DamageNumber|FX")
    float HorizontalJitter = 0.f; // 좌우 살짝 흔들림(옵션)

    UPROPERTY(EditDefaultsOnly, Category = "DamageNumber|FX")
    float StartScale = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "DamageNumber|FX")
    float EndScale = 0.9f;

    // BP 에서 스케일 커브로 튜닝하고 싶으면 지정(0~1 구간)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|FX")
    TObjectPtr<UCurveFloat> ScaleCurve = nullptr;

    // BP에서 쉽게 바꿀 수 있는 색/폰트 (일반/크리티컬)

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DamageNumber|Style")
    int32 FontSize_Crit = 32;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DamageNumber|Style")
    int32 OutlineSize = 2;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DamageNumber|Style")
    FLinearColor OutlineColor = FLinearColor::Black;

    float Age = 0.f;
    FVector BaseLoc;

private:
    float PendingDamage = 0.f;
    FLinearColor PendingColor = FLinearColor::White;
    int32 PendingFontSize = 28;


    void UpdateVisual(float T);

    bool bPendingLabel = false;
    FText PendingLabel;

};
