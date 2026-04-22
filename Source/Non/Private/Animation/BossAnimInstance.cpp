#include "Animation/BossAnimInstance.h"
#include "Character/BossCharacter.h"

void UBossAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    // 부모 클래스의 기본 이동 변수(Speed, bInAir 등) 갱신 로직 실행
    Super::NativeUpdateAnimation(DeltaSeconds);

    // 보스 전용 상태값 동기화
    if (!BossChar.IsValid())
    {
        BossChar = Cast<ABossCharacter>(TryGetPawnOwner());
    }

    if (BossChar.IsValid())
    {
        BossPhase = BossChar->CurrentPhase;
        bIsTransitioning = BossChar->bIsTransitioningPhase;
    }
}
