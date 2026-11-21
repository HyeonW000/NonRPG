#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BossAnimInstance.generated.h"

UCLASS()
class NON_API UBossAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadOnly, Category = "BossAnim")
    int32 BossPhase = 1;

    UPROPERTY(BlueprintReadOnly, Category = "BossAnim")
    bool bIsTransitioning = false;

    UPROPERTY(BlueprintReadOnly, Category = "BossAnim")
    float Speed = 0.f;

    // ───── 보스 헤드(시선) 어그로 타겟 트래킹 (ABP_Boss_Base / ABP_StoneGolem 연동용) ─────
    // ★ 중요: 턴/공격 몽타주 재생 중에도 머리가 툭 풀리지 않고 자연스럽게 유지되려면
    // AnimGraph에서 반드시 [Slot 'DefaultSlot'] 뒤쪽(Output Pose 직전)에 Transform (Modify) Bone 노드를 배치하세요!
    UPROPERTY(BlueprintReadOnly, Category = "BossAnim|LookAt")
    FRotator HeadLookAtRotation = FRotator::ZeroRotator;

    // Look At 노드의 Look At Location (World Space)에 연결할 어그로 타겟의 위치
    UPROPERTY(BlueprintReadOnly, Category = "BossAnim|LookAt")
    FVector HeadLookAtLocation = FVector::ZeroVector;

    // Look At 또는 Transform (Modify) Bone 노드의 Alpha에 연결할 가중치
    UPROPERTY(BlueprintReadOnly, Category = "BossAnim|LookAt")
    float HeadLookAtAlpha = 0.0f;

    // 머리 좌우 최대 회전 각도 (기본 35도 - 과하게 꺾이지 않도록 자연스러운 범위로 제한)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossAnim|LookAt")
    float MaxHeadYawAngle = 35.0f;

    // 머리 상하 최대 회전 각도 (기본 10도 - 골렘 등 거대 보스의 턱이 가슴에 파묻히지 않도록 자연스러운 각도 유지)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossAnim|LookAt")
    float MaxHeadPitchAngle = 10.0f;

    // 플레이어를 바라볼 때의 시선 회전 보간 속도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossAnim|LookAt")
    float HeadLookAtInterpSpeed = 5.0f;

    // 시선이 정면(0도)으로 복귀할 때의 보간 속도 (툭 돌아가지 않도록 더 부드럽게 복귀)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossAnim|LookAt")
    float HeadLookAtReturnSpeed = 3.0f;

protected:
    TWeakObjectPtr<class ABossCharacter> BossChar;
    float CurrentLookYaw = 0.0f;
    float CurrentLookPitch = 0.0f;
};
