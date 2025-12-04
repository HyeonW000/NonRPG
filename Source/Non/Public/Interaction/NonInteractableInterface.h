#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NonInteractableInterface.generated.h"

class ANonCharacterBase;

// UINTERFACE
UINTERFACE(Blueprintable)
class NON_API UNonInteractableInterface : public UInterface
{
    GENERATED_BODY()
};

// 실제 인터페이스
class NON_API INonInteractableInterface
{
    GENERATED_BODY()

public:
    // 상호작용 함수
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    void Interact(ANonCharacterBase* Interactor);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    FText GetInteractLabel();
};
