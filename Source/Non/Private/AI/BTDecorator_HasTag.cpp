#include "AI/BTDecorator_HasTag.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

UBTDecorator_HasTag::UBTDecorator_HasTag()
{
    NodeName = TEXT("Has Gameplay Tag?");
}

bool UBTDecorator_HasTag::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    APawn* Pawn = AIC ? AIC->GetPawn() : nullptr;

    if (!Pawn) return bInvertCondition;

    // 액터로부터 ASC(Ability System Component)를 가져와 태그를 확인합니다.
    UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
    if (!ASC) return bInvertCondition;

    // 태그 소유 여부 확인
    bool bHasTag = ASC->HasMatchingGameplayTag(TargetTag);

    // 반전 조건 처리 (태그가 없을 때를 원하면 결과 뒤집기)
    return bInvertCondition ? !bHasTag : bHasTag;
}
