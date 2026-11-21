$filePath = 'D:\Non\Source\Non\Private\Character\NonCharacterBase.cpp'
$lines = [System.IO.File]::ReadAllLines($filePath)

# 1. Update lines 994..1000 (0-based) for bHasMoveInput
$newLinesTop = @(
    "    const FVector InputVec = GetLastMovementInputVector();",
    "    const bool bHasMoveInput = (InputVec.SizeSquared() > KINDA_SMALL_NUMBER);"
)

# 2. Update lines 3435..3525 (0-based) for UpdateGuardTurnInPlace
$newTurnFunction = @(
"void ANonCharacterBase::UpdateGuardTurnInPlace(float DeltaSeconds) {",
"  if (!bGuarding) {",
"    bIsGuardTurning = false;",
"    return;",
"  }",
"",
"  const FRotator CurrentActorRot = GetActorRotation();",
"  const FRotator ControlRot = GetControlRotation();",
"",
"  // 컨트롤러(카메라)와 액터 간의 Yaw 각도 차이 (-180 ~ +180)",
"  const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentActorRot.Yaw, ControlRot.Yaw);",
"",
"  UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;",
"  if (!AnimInst)",
"    return;",
"",
"  // 1) 이미 턴 몽타주가 재생 중인 경우 -> 부드럽게 목표 카메라 각도로 액터 회전 보간",
"  if (bIsGuardTurning) {",
"    GuardTurnElapsed += DeltaSeconds;",
"    const float TotalDuration = FMath::Max(0.2f, GuardTurnMontageDuration);",
"    const float Progress = FMath::Clamp(GuardTurnElapsed / TotalDuration, 0.f, 1.0f);",
"",
"    // 마우스를 계속 움직이고 있다면 목표 각도를 카메라 방향으로 자연스럽게 추적",
"    TargetGuardTurnYaw = ControlRot.Yaw;",
"",
"    // 부드러운 회전 보간 (진행도에 따라 부드럽게 가속-감속하여 목표 방향 도달)",
"    const float SmoothAlpha = FMath::SmoothStep(0.f, 1.f, Progress);",
"    const float InterpSpeed = FMath::Lerp(10.f, 22.f, SmoothAlpha);",
"    const float NewYaw = FMath::FInterpTo(CurrentActorRot.Yaw, TargetGuardTurnYaw, DeltaSeconds, InterpSpeed);",
"",
"    FRotator NewRot = CurrentActorRot;",
"    NewRot.Yaw = FRotator::NormalizeAxis(NewYaw);",
"    SetActorRotation(NewRot);",
"",
"    // 화면 실시간 디버그 메시지 (사용자 디버깅용)",
"    if (GEngine && IsLocallyControlled()) {",
"      const FString DebugMsg = FString::Printf(TEXT(\"[가드 턴 진행 중] 각도차: %+.1f° | 진행도: %.0f%% (경과: %.2fs / %.2fs)\"),",
"          DeltaYaw, Progress * 100.f, GuardTurnElapsed, TotalDuration);",
"      GEngine->AddOnScreenDebugMessage(1101, 0.1f, FColor::Green, DebugMsg);",
"    }",
"",
"    // 몽타주 실제 재생 여부 확인",
"    const bool bMontageStillPlaying = (",
"        (GuardTurnLeft90Montage && AnimInst->Montage_IsPlaying(GuardTurnLeft90Montage)) ||",
"        (GuardTurnRight90Montage && AnimInst->Montage_IsPlaying(GuardTurnRight90Montage)) ||",
"        (GuardTurnLeft180Montage && AnimInst->Montage_IsPlaying(GuardTurnLeft180Montage)) ||",
"        (GuardTurnRight180Montage && AnimInst->Montage_IsPlaying(GuardTurnRight180Montage)));",
"",
"    // 최소 0.4초 이상 경과하고 몽타주가 끝났거나, 진행도가 100%에 도달했으면 턴 완료",
"    if (Progress >= 1.0f || (!bMontageStillPlaying && GuardTurnElapsed >= 0.4f)) {",
"      bIsGuardTurning = false;",
"      if (GEngine && IsLocallyControlled()) {",
"        GEngine->AddOnScreenDebugMessage(1102, 1.5f, FColor::Cyan, TEXT(\"[가드 턴 완료] 1회 회전 완료 및 정렬\"));",
"      }",
"    }",
"    return;",
"  }",
"",
"  // 2) 턴 몽타주 발동 여부 검사 (로컬 플레이어가 주도적으로 각도 감지 및 발동)",
"  if (!IsLocallyControlled()) {",
"    return;",
"  }",
"",
"  const float AbsDeltaYaw = FMath::Abs(DeltaYaw);",
"",
"  // 가드 대기 상태 실시간 각도 디버그 표시",
"  if (GEngine && IsLocallyControlled()) {",
"    const FString IdleMsg = FString::Printf(TEXT(\"[가드 대기] 카메라 각도차: %+.1f° (발동 기준: 50.0°)\"), DeltaYaw);",
"    GEngine->AddOnScreenDebugMessage(1101, 0.1f, FColor::White, IdleMsg);",
"  }",
"",
"  if (AbsDeltaYaw < GuardTurnThresholdAngle) {",
"    return; // 아직 턴 임계값(50도) 미만",
"  }",
"",
"  // 발동할 몽타주 선정",
"  UAnimMontage* MontageToPlay = nullptr;",
"  if (AbsDeltaYaw >= GuardTurn180ThresholdAngle) {",
"    MontageToPlay = (DeltaYaw < 0.f) ? GuardTurnLeft180Montage : GuardTurnRight180Montage;",
"    if (!MontageToPlay) {",
"      MontageToPlay = (DeltaYaw < 0.f) ? GuardTurnLeft90Montage : GuardTurnRight90Montage;",
"    }",
"  } else {",
"    MontageToPlay = (DeltaYaw < 0.f) ? GuardTurnLeft90Montage : GuardTurnRight90Montage;",
"  }",
"",
"  if (MontageToPlay) {",
"    const float PlayDuration = PlayAnimMontage(MontageToPlay);",
"    if (PlayDuration > 0.f) {",
"      bIsGuardTurning = true;",
"      StartGuardTurnYaw = CurrentActorRot.Yaw;",
"      TargetGuardTurnYaw = ControlRot.Yaw;",
"      GuardTurnMontageDuration = PlayDuration;",
"      GuardTurnElapsed = 0.f;",
"",
"      UE_LOG(LogTemp, Log, TEXT(\"[가드 턴 발동] 몽타주: %s (길이: %.2f초) | 목표각도: %.1f | 각도차: %.1f\"),",
"          *MontageToPlay->GetName(), PlayDuration, TargetGuardTurnYaw, DeltaYaw);",
"",
"      if (GEngine && IsLocallyControlled()) {",
"        const FString StartMsg = FString::Printf(TEXT(\"[가드 턴 발동!] %s (각도차: %+.1f°)\"), *MontageToPlay->GetName(), DeltaYaw);",
"        GEngine->AddOnScreenDebugMessage(1102, 1.5f, FColor::Yellow, StartMsg);",
"      }",
"",
"      // 몽타주 종료 델리게이트 바인딩",
"      FOnMontageEnded EndDelegate;",
"      EndDelegate.BindUObject(this, &ANonCharacterBase::OnGuardTurnMontageEnded);",
"      AnimInst->Montage_SetEndDelegate(EndDelegate, MontageToPlay);",
"",
"      // 서버 및 다른 클라이언트에 턴 동기화",
"      if (!HasAuthority()) {",
"        Server_PlayGuardTurn(MontageToPlay, TargetGuardTurnYaw);",
"      } else {",
"        Multicast_PlayGuardTurn(MontageToPlay, TargetGuardTurnYaw);",
"      }",
"    }",
"  }",
"}"
)

$outList = [System.Collections.Generic.List[string]]::new()
for ($i = 0; $i -lt $lines.Length; $i++) {
    if ($i -ge 994 -and $i -le 1000) {
        if ($i -eq 994) {
            foreach ($nl in $newLinesTop) { $outList.Add($nl) }
        }
        continue
    }
    if ($i -ge 3435 -and $i -le 3525) {
        if ($i -eq 3435) {
            foreach ($tf in $newTurnFunction) { $outList.Add($tf) }
        }
        continue
    }
    $outList.Add($lines[$i])
}

[System.IO.File]::WriteAllLines($filePath, $outList, [System.Text.Encoding]::UTF8)
Write-Output "Successfully updated NonCharacterBase.cpp"

