#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "System/PartyTypes.h"
#include "PartyComponent.generated.h"

class ANonPlayerController;
class ANonCharacterBase;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class NON_API UPartyComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPartyComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── 델리게이트 (UI 및 블루프린트 연동) ──
    UPROPERTY(BlueprintAssignable, Category = "Party|Events")
    FOnPartyUpdated OnPartyUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Party|Events")
    FOnPartyInviteReceived OnPartyInviteReceived;

    UPROPERTY(BlueprintAssignable, Category = "Party|Events")
    FOnPartyLeft OnPartyLeft;

    UPROPERTY(BlueprintAssignable, Category = "Party|Events")
    FOnPartyInviteDeclined OnPartyInviteDeclined;

    UPROPERTY(BlueprintAssignable, Category = "Party|Events")
    FOnPartyMemberKicked OnPartyMemberKicked;

    // ── 상태 조회 함수 ──
    UFUNCTION(BlueprintPure, Category = "Party")
    bool IsInParty() const { return CurrentParty.IsInParty(); }

    UFUNCTION(BlueprintPure, Category = "Party")
    bool IsPartyLeader() const;

    UFUNCTION(BlueprintPure, Category = "Party")
    const FPartyData& GetPartyData() const { return CurrentParty; }

    UFUNCTION(BlueprintPure, Category = "Party")
    int32 GetPartyMemberCount() const { return CurrentParty.GetMemberCount(); }

    /** 대상 플레이어에게 파티 초대를 보낼 수 있는지 사전 검사 (실패 사유 OutFailReason 반환) */
    bool CanInvite(ANonPlayerController* TargetPC, FString& OutFailReason) const;

    // ── 플레이어 인터랙션 명령 ──
    /** 다른 플레이어에게 파티 초대 요청 (클라이언트에서 호출 가능) */
    UFUNCTION(BlueprintCallable, Category = "Party")
    void RequestInviteParty(ANonPlayerController* TargetPC);

    /** 대상 캐릭터 액터로 파티 초대 요청 (클라이언트에서도 안전하게 작동) */
    UFUNCTION(BlueprintCallable, Category = "Party")
    void RequestInvitePartyByCharacter(ANonCharacterBase* TargetChar);

    /** 파티 초대 응답 (수락 / 거절) */
    UFUNCTION(BlueprintCallable, Category = "Party")
    void RespondToPartyInvite(bool bAccept);

    void SetPendingInviterPC(ANonPlayerController* InPC);

    /** 파티 탈퇴 */
    UFUNCTION(BlueprintCallable, Category = "Party")
    void LeaveParty();

    /** 파티원 추방 (파티장 전용) */
    UFUNCTION(BlueprintCallable, Category = "Party")
    void KickPartyMember(const FString& TargetNickname);

    /** 파티 초대 서버 내부 처리 로직 (서버 RPC 및 C++ 내부에서 직접 호출 가능) */
    void InvitePartyInternal(ANonPlayerController* TargetPC);

    // ── 네트워크 RPC (서버 / 클라이언트) ──
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_InviteParty(ANonPlayerController* TargetPC);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_InvitePartyByCharacter(ANonCharacterBase* TargetChar);

    UFUNCTION(Client, Reliable)
    void Client_ReceivePartyInvite(const FString& InviterNickname, ANonPlayerController* InviterPC);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RespondPartyInvite(bool bAccept);

    UFUNCTION(Client, Reliable)
    void Client_PartyInviteDeclined(const FString& TargetNickname);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_LeaveParty();

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_KickPartyMember(const FString& TargetNickname);

    /** 서버에서 파티 데이터를 파티원들에게 동기화 전송 */
    UFUNCTION(Client, Reliable)
    void Client_SyncPartyData(const FPartyData& NewPartyData);

    /** 파티 해산 알림 */
    UFUNCTION(Client, Reliable)
    void Client_PartyDisbanded();

    /** 파티원 스탯 주기적 갱신 RPC */
    UFUNCTION(Server, Unreliable)
    void Server_SyncMyStats(float CurrentHP, float MaxHP, float CurrentMP, float MaxMP);

protected:
    UPROPERTY(ReplicatedUsing = OnRep_PartyData, BlueprintReadOnly, Category = "Party")
    FPartyData CurrentParty;

    UFUNCTION()
    void OnRep_PartyData();

    /** 나에게 온 최근 초대자 캐싱 */
    UPROPERTY()
    TWeakObjectPtr<ANonPlayerController> PendingInviterPC = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Party")
    int32 MaxPartyMembers = 4;

    float StatSyncTimer = 0.f;
    const float StatSyncInterval = 0.5f; // 0.5초마다 파티원에게 내 체력/마나 동기화
};
