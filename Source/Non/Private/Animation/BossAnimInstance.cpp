#include "Animation/BossAnimInstance.h"
#include "Character/BossCharacter.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"

void UBossAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!BossChar.IsValid())
    {
        BossChar = Cast<ABossCharacter>(TryGetPawnOwner());
    }

    if (BossChar.IsValid())
    {
        BossPhase = BossChar->CurrentPhase;
        bIsTransitioning = BossChar->bIsTransitioningPhase;

        // [New] 제자리 회전(Turn In Place)을 위한 각도 계산
        // 컨트롤러의 회전을 거치지 않고 블랙보드 타겟의 위치를 직접 참조하여 100% 실시간성을 확보합니다.
        if (UBlackboardComponent* BB = UAIBlueprintHelperLibrary::GetBlackboard(BossChar.Get()))
        {
            if (AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor"))))
            {
                const FRotator ActorRotation = BossChar->GetActorRotation();
                FVector Dir = TargetActor->GetActorLocation() - BossChar->GetActorLocation();
                Dir.Z = 0.f;
                const FRotator TargetRotation = Dir.Rotation();
                
                // 두 각도의 차이를 구하고 -180 ~ 180 사이로 정규화합니다.
                const FRotator Delta = (TargetRotation - ActorRotation).GetNormalized();
                RootYawOffset = Delta.Yaw;
            }
            else
            {
                RootYawOffset = 0.f;
            }
        }
    }
}
