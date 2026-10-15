#include "AI/BTService_UpdateTarget.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "Character/EnemyCharacter.h"
#include "GameFramework/Pawn.h"
#include "Character/NonCharacterBase.h"
#include "Combat/CombatSlotComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"

UBTService_UpdateTarget::UBTService_UpdateTarget()
{
    bNotifyTick = true;
    Interval = 0.20f;     // 감지 주기 (0.1~0.3 추천)
    RandomDeviation = 0.f;
    NodeName = TEXT("Update Target (C++)");

    TargetActorKey.SelectedKeyName = TEXT("TargetActor");
    DistanceKey.SelectedKeyName = TEXT("DistanceToTarget");
    AngleKey.SelectedKeyName = TEXT("TargetAngle");
}

void UBTService_UpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    AAIController* AIC = OwnerComp.GetAIOwner();
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!AIC || !BB) return;

    AEnemyCharacter* Self = Cast<AEnemyCharacter>(AIC->GetPawn());
    if (!Self) return;

    if (Self->IsSpawnFading())
    {
        return;
    }

    UWorld* World = AIC->GetWorld();
    if (!World) return;
    const float Now = World->GetTimeSeconds();

    // 안전한 블랙보드 키 해석 (에디터 디테일 패널에서 미선택 시에도 기본 이름으로 완벽 폴백)
    const FName ActualTargetKey = (!TargetActorKey.IsNone() && TargetActorKey.SelectedKeyName != NAME_None)
        ? TargetActorKey.SelectedKeyName
        : FName(TEXT("TargetActor"));

    AActor* Curr = Cast<AActor>(BB->GetValueAsObject(ActualTargetKey));
    if (!Curr)
    {
        Curr = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
    }

    const bool bReactiveMode =
        bRespectAggroStyle ? (Self->AggroStyle == EAggroStyle::Reactive) : false;

    // ── 1) 기존 타겟 유지/해제 검사 ───────────────────────
    if (Curr)
    {
        bool bLoseTarget = false;
        FString LoseReason;
        const float DistToCurr = FVector::Dist2D(Self->GetActorLocation(), Curr->GetActorLocation());

        // 0) 플레이어가 죽었는지 검사
        if (ANonCharacterBase* TargetChar = Cast<ANonCharacterBase>(Curr)) {
            if (TargetChar->IsDead()) {
                bLoseTarget = true;
                LoseReason = TEXT("플레이어 사망");
            }
        }

        // 1) 플레이어가 ExitRadius 밖으로 나감 + 최소 유지시간
        if (DistToCurr > ExitRadius && (Now - LastSwitchTime) >= MinHoldTimeOnExit)
        {
            bLoseTarget = true;
            LoseReason = FString::Printf(TEXT("이탈거리 초과(%.1f > %.1f)"), DistToCurr, ExitRadius);
        }

        // 2) Reactive 모드: 맞아서 생긴 어그로가 만료되었는지
        if (bReactiveMode)
        {
            const bool bAggroFlag = Self->IsAggroByHit();
            const float TimeSinceHit = Now - Self->GetLastAggroByHitTime();
            const bool bAggroExpired = (!bAggroFlag) || (TimeSinceHit > Self->GetAggroByHitHoldTime());

            if (bAggroExpired)
            {
                bLoseTarget = true;
                LoseReason = TEXT("피격 어그로 만료");
            }
        }

        // 3) 홈 리쉬: 스폰 지점에서 너무 멀어졌는지
        if (bUseHomeLeash)
        {
            const float DistFromHome = FVector::Dist2D(Self->GetActorLocation(), Self->SpawnLocation);
            if (DistFromHome > HomeLeashRadius)
            {
                bLoseTarget = true;
                Self->bIsLeashingHome = true; // ★ 스폰 지점 복귀 모드 돌입!
                LoseReason = FString::Printf(TEXT("홈리쉬 초과(스폰거리 %.1f > %.1f)"), DistFromHome, HomeLeashRadius);
            }
        }

        // 4) 전투 무반응 타임아웃: 서로 아무 피해도 주고받지 못한 시간이 지속될 경우 (기본 8초)
        if (!bLoseTarget && CombatInactivityTimeout > 0.0f && Self->LastCombatActionTime > 0.0f)
        {
            const float InactiveDuration = Now - Self->LastCombatActionTime;
            if (InactiveDuration >= CombatInactivityTimeout)
            {
                bLoseTarget = true;
                Self->bIsLeashingHome = true;
                LoseReason = FString::Printf(TEXT("전투 무반응 타임아웃(%.1f초 동안 전투 중단)"), InactiveDuration);
            }
        }

        if (bLoseTarget)
        {
            // 타겟을 잃었을 때 기존 타겟의 포위 슬롯 반환
            if (Curr)
            {
                if (UCombatSlotComponent* SlotComp = Curr->FindComponentByClass<UCombatSlotComponent>())
                {
                    SlotComp->ReleaseSlot(Self);
                }
            }

            BB->ClearValue(ActualTargetKey);
            BB->ClearValue(TEXT("TargetActor"));
            BB->ClearValue(TEXT("TargetLocation"));
            BB->ClearValue(TEXT("DistanceToTarget"));
            BB->ClearValue(TEXT("Distance"));
            BB->ClearValue(TEXT("TargetAngle"));
            BB->ClearValue(TEXT("Angle"));
            Self->SetAggro(false);
            LastSwitchTime = Now;
            Curr = nullptr; // 신규 타겟을 즉시 찾도록 비움

            if (GEngine)
            {
                const FString DebugMsg = FString::Printf(
                    TEXT("[AI 디버그] %s | [타겟 이탈: %s] 스폰 위치로 복귀 시작"),
                    *Self->GetName(), *LoseReason
                );
                GEngine->AddOnScreenDebugMessage((uint64)Self->GetUniqueID(), 1.5f, FColor::Silver, DebugMsg);
                UE_LOG(LogTemp, Warning, TEXT("%s"), *DebugMsg);
            }
        }
        else
        {
            // 포위 슬롯 예약 및 실시간 슬롯 위치 갱신
            if (UCombatSlotComponent* SlotComp = Curr->FindComponentByClass<UCombatSlotComponent>())
            {
                SlotComp->ReserveSlot(Self);
                const FVector SlotPos = SlotComp->GetSlotLocation(Self);
                BB->SetValueAsVector(TEXT("TargetLocation"), SlotPos);
            }

            // 실시간 거리/각도 기록
            if (!DistanceKey.IsNone())
            {
                BB->SetValueAsFloat(DistanceKey.SelectedKeyName, DistToCurr);
            }
            if (!AngleKey.IsNone())
            {
                float AbsAngle = FMath::Abs(Self->GetAngleToTarget(Curr));
                BB->SetValueAsFloat(AngleKey.SelectedKeyName, AbsAngle);
            }

            // 일반적인 블랙보드 키 이름들에 자동 동기화 (블랙보드 데코레이터가 항상 올바른 값을 읽도록 보장)
            BB->SetValueAsFloat(TEXT("DistanceToTarget"), DistToCurr);
            BB->SetValueAsFloat(TEXT("Distance"), DistToCurr);
            const float AbsAngle = FMath::Abs(Self->GetAngleToTarget(Curr));
            BB->SetValueAsFloat(TEXT("TargetAngle"), AbsAngle);
            BB->SetValueAsFloat(TEXT("Angle"), AbsAngle);

            if (GEngine && DistToCurr > 400.f)
            {
                const FString DebugMsg = FString::Printf(
                    TEXT("[AI 디버그] %s | 현재거리: %.1f cm | 사정거리: %.1f cm | 각도: %.1f° | 상태: CHASING (원거리 추적 중)"),
                    *Self->GetName(), DistToCurr, Self->AttackRange, AbsAngle
                );
                GEngine->AddOnScreenDebugMessage((uint64)Self->GetUniqueID(), 0.25f, FColor::Orange, DebugMsg);
            }

            // 타겟을 정상 유지하고 있으면 달리기 속도 유지 후 종료
            if (UCharacterMovementComponent* MoveComp = Self->GetCharacterMovement())
            {
                MoveComp->MaxWalkSpeed = (Self->CombatRunSpeed > 0.0f) ? Self->CombatRunSpeed : CombatRunSpeed;
            }
            return;
        }
    }

    // ── 2) 멀티플레이 신규 타겟 획득 (가장 가까운 살아있는 플레이어 탐색) ─────────
    if (!Curr && (Now - LastSwitchTime) >= MinHoldTimeOnEnter)
    {
        // ★ [핵심: MMORPG형 2단계 스캔 복귀 파이프라인]
        // 1단계: 스폰 위치 기준 1300cm 이내 진입 시 ➔ 1차 스캔 (뒤따라온 플레이어가 있으면 복귀 즉시 취소 후 유턴)
        // 2단계: 스폰 위치 도착 완료(150cm 이내) 시 ➔ 2차 스캔 (가만히 서 있는 플레이어 확실한 즉시 감지)
        if (Self->bIsLeashingHome)
        {
            const float DistFromHome = FVector::Dist2D(Self->GetActorLocation(), Self->SpawnLocation);
            const bool bReachedHomeZone = (DistFromHome <= HomeLeashReleaseRadius); // 1차 스캔 구역 진입
            const bool bFullyArrivedHome = (DistFromHome <= 200.0f);                // 2차 완전 복귀 완료
            const bool bHitByPlayer = Self->IsAggroByHit();

            if (bReachedHomeZone || bFullyArrivedHome || bHitByPlayer)
            {
                Self->bIsLeashingHome = false;
                if (GEngine)
                {
                    const FString ScanStage = bHitByPlayer ? TEXT("피격 반격") : (bFullyArrivedHome ? TEXT("2차 스캔: 스폰 완전 도착") : TEXT("1차 스캔: 홈 구역 진입"));
                    const FString DebugMsg = FString::Printf(
                        TEXT("[AI 디버그] %s | [%s] 복귀 락 해제 (스폰거리: %.1f cm) -> 주변 감시 즉각 가동!"),
                        *Self->GetName(), *ScanStage, DistFromHome
                    );
                    GEngine->AddOnScreenDebugMessage((uint64)Self->GetUniqueID(), 1.5f, FColor::Green, DebugMsg);
                    UE_LOG(LogTemp, Warning, TEXT("%s"), *DebugMsg);
                }
            }
            else
            {
                // 아직 홈 구역 밖에서 이동 중이면 신규 타겟팅 차단
                return;
            }
        }

        AActor* BestTarget = nullptr;
        float BestDist = EnterRadius;

        // 접속한 모든 플레이어 컨트롤러를 순회하여 가장 가까운 플레이어를 탐색!
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            if (APlayerController* PC = It->Get())
            {
                if (APawn* TargetPawn = PC->GetPawn())
                {
                    if (ANonCharacterBase* PlayerChar = Cast<ANonCharacterBase>(TargetPawn))
                    {
                        if (!PlayerChar->IsDead())
                        {
                            const float Dist = FVector::Dist2D(Self->GetActorLocation(), PlayerChar->GetActorLocation());
                            if (Dist < BestDist)
                            {
                                BestDist = Dist;
                                BestTarget = PlayerChar;
                            }
                        }
                    }
                }
            }
        }

        if (BestTarget)
        {
            bool bCanAggro = true;

            if (bReactiveMode)
            {
                const bool bAggroFlag = Self->IsAggroByHit();
                const float TimeSinceHit = Now - Self->GetLastAggroByHitTime();
                const bool bWithinHold = (TimeSinceHit <= Self->GetAggroByHitHoldTime());
                bCanAggro = (bAggroFlag && bWithinHold);
            }

            if (bCanAggro)
            {
                // ★ [순서 제어] 선공(Preemptive) 몬스터이고 아직 어그로가 끌리지 않은 상태라면:
                // 먼저 1단계(이벤트 발동) -> 2단계(반응 텀)를 실행하고, 텀 동안에는 타겟을 채우지 않고 제자리 대기!
                if (Self->AggroStyle == EAggroStyle::Preemptive && !Self->IsAggro())
                {
                    if (!Self->bIsAlerting)
                    {
                        Self->TriggerAlertReaction(BestTarget);
                    }
                    // 반응 텀이 끝날 때까지는 비헤이비어 트리가 플레이어에게 접근(MoveTo)하지 않도록 대기
                    return;
                }

                // 경계 텀 진행 중이면 여전히 접근 대기
                if (Self->bIsAlerting)
                {
                    return;
                }

                // ★ [3단계: 플레이어 접근] 반응 텀이 종료되었으므로 비로소 TargetActor를 채우고 본격 추격 돌입!
                BB->SetValueAsObject(ActualTargetKey, BestTarget);
                BB->SetValueAsObject(TEXT("TargetActor"), BestTarget);

                // 신규 타겟 포위 슬롯 예약 및 위치 초기화
                if (UCombatSlotComponent* SlotComp = BestTarget->FindComponentByClass<UCombatSlotComponent>())
                {
                    SlotComp->ReserveSlot(Self);
                    const FVector SlotPos = SlotComp->GetSlotLocation(Self);
                    BB->SetValueAsVector(TEXT("TargetLocation"), SlotPos);
                }

                LastSwitchTime = Now;

                // 어그로 시작 플래그
                Self->SetAggro(true);

                // 사정거리 진입 시각 기록 (웜업용)
                Self->MarkEnteredAttackRange();

                if (!DistanceKey.IsNone())
                {
                    BB->SetValueAsFloat(DistanceKey.SelectedKeyName, BestDist);
                }
                if (!AngleKey.IsNone())
                {
                    float AbsAngle = FMath::Abs(Self->GetAngleToTarget(BestTarget));
                    BB->SetValueAsFloat(AngleKey.SelectedKeyName, AbsAngle);
                }

                BB->SetValueAsFloat(TEXT("DistanceToTarget"), BestDist);
                BB->SetValueAsFloat(TEXT("Distance"), BestDist);
                const float AbsAngle = FMath::Abs(Self->GetAngleToTarget(BestTarget));
                BB->SetValueAsFloat(TEXT("TargetAngle"), AbsAngle);
                BB->SetValueAsFloat(TEXT("Angle"), AbsAngle);

                if (ANonCharacterBase* PlayerChar = Cast<ANonCharacterBase>(BestTarget))
                {
                    PlayerChar->EnterCombatState();
                }

                if (GEngine)
                {
                    const FString DebugMsg = FString::Printf(
                        TEXT("[AI 디버그] %s | [플레이어 재발견!] 즉시 추적 전환 (거리: %.1f cm)"),
                        *Self->GetName(), BestDist
                    );
                    GEngine->AddOnScreenDebugMessage((uint64)Self->GetUniqueID(), 1.5f, FColor::Emerald, DebugMsg);
                    UE_LOG(LogTemp, Warning, TEXT("%s"), *DebugMsg);
                }
            }
        }
    }

    // ── 3) 어그로/복귀 유무에 따른 보스/몬스터 개별 이동 속도(Walk vs Run) 자동 조절 ─────────
    if (UCharacterMovementComponent* MoveComp = Self->GetCharacterMovement())
    {
        // 몬스터 개별 캐릭터 블루프린트에 지정된 속도가 있으면 최우선 반영!
        float ActualRunSpeed = (Self->CombatRunSpeed > 0.0f) ? Self->CombatRunSpeed : CombatRunSpeed;
        float ActualWalkSpeed = (Self->PatrolWalkSpeed > 0.0f) ? Self->PatrolWalkSpeed : PatrolWalkSpeed;

        AActor* FinalTarget = Cast<AActor>(BB->GetValueAsObject(ActualTargetKey));
        if (!FinalTarget)
        {
            FinalTarget = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor")));
        }
        if (FinalTarget)
        {
            if (Self->bIsAlerting)
            {
                // 경계 텀 동안에는 플레이어를 응시하며 제자리 정지
                MoveComp->MaxWalkSpeed = 0.0f;
            }
            else
            {
                // 어그로 잡힘 ➔ 해당 몬스터 고유 달리기 속도 적용
                MoveComp->MaxWalkSpeed = ActualRunSpeed;
            }
        }
        else
        {
            // 타겟 없음: 이전에 어그로가 끌렸다가 스폰 지역으로 리셋 복귀 중이면 달리기(Run), 스폰 구역 근처나 평상시 순찰 시에는 걷기(Walk)
            float DistFromHome = FVector::Dist2D(Self->GetActorLocation(), Self->SpawnLocation);
            if (Self->IsAggro() && DistFromHome > 300.0f)
            {
                MoveComp->MaxWalkSpeed = ActualRunSpeed; // 리셋 빠른 복귀
            }
            else
            {
                MoveComp->MaxWalkSpeed = ActualWalkSpeed; // 평상시 순찰 걷기 (Patrol)
            }

            if (GEngine)
            {
                const FString DebugMsg = FString::Printf(
                    TEXT("[AI 디버그] %s | 상태: RETURNING / PATROL (스폰 거리: %.1f cm)"),
                    *Self->GetName(), DistFromHome
                );
                GEngine->AddOnScreenDebugMessage((uint64)Self->GetUniqueID(), 0.25f, FColor::Silver, DebugMsg);
            }
        }
    }
}

