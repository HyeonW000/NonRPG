#include "System/PartyComponent.h"
#include "Core/NonPlayerController.h"
#include "Character/NonCharacterBase.h"
#include "Ability/NonAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

UPartyComponent::UPartyComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UPartyComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UPartyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UPartyComponent, CurrentParty);
}

void UPartyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 로컬 컨트롤러이고 파티에 속해있을 때 실시간 체력/마나를 주기적으로 서버에 동기화
    if (CurrentParty.IsInParty())
    {
        ANonPlayerController* PC = Cast<ANonPlayerController>(GetOwner());
        if (PC && PC->IsLocalController())
        {
            StatSyncTimer += DeltaTime;
            if (StatSyncTimer >= StatSyncInterval)
            {
                StatSyncTimer = 0.f;

                if (ANonCharacterBase* Char = Cast<ANonCharacterBase>(PC->GetPawn()))
                {
                    if (const UNonAttributeSet* AS = Char->GetAttributeSet())
                    {
                        Server_SyncMyStats(AS->GetHP(), AS->GetMaxHP(), AS->GetMP(), AS->GetMaxMP());
                    }
                }
            }
        }
    }
}

void UPartyComponent::OnRep_PartyData()
{
    if (CurrentParty.IsInParty())
    {
        OnPartyUpdated.Broadcast(CurrentParty);
    }
    else
    {
        OnPartyLeft.Broadcast();
    }
}

bool UPartyComponent::IsPartyLeader() const
{
    if (!CurrentParty.IsInParty()) return false;
    
    ANonPlayerController* PC = Cast<ANonPlayerController>(GetOwner());
    if (!PC) return false;

    return CurrentParty.LeaderNickname.Equals(PC->GetPlayerNickname());
}

void UPartyComponent::RequestInviteParty(ANonPlayerController* TargetPC)
{
    Server_InviteParty(TargetPC);
}

void UPartyComponent::RequestInvitePartyByCharacter(ANonCharacterBase* TargetChar)
{
    Server_InvitePartyByCharacter(TargetChar);
}

bool UPartyComponent::Server_InvitePartyByCharacter_Validate(ANonCharacterBase* TargetChar)
{
    return true;
}

void UPartyComponent::Server_InvitePartyByCharacter_Implementation(ANonCharacterBase* TargetChar)
{
    if (!TargetChar) return;
    ANonPlayerController* TargetPC = Cast<ANonPlayerController>(TargetChar->GetController());
    if (!TargetPC && GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (ANonPlayerController* NonPC = Cast<ANonPlayerController>(It->Get()))
            {
                if (NonPC->GetPawn() == TargetChar)
                {
                    TargetPC = NonPC;
                    break;
                }
            }
        }
    }

    if (TargetPC)
    {
        Server_InviteParty(TargetPC);
    }
}

bool UPartyComponent::Server_InviteParty_Validate(ANonPlayerController* TargetPC)
{
    return true;
}

void UPartyComponent::Server_InviteParty_Implementation(ANonPlayerController* TargetPC)
{
    InvitePartyInternal(TargetPC);
}

bool UPartyComponent::CanInvite(ANonPlayerController* TargetPC, FString& OutFailReason) const
{
    ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwner());
    if (!MyPC || !TargetPC)
    {
        OutFailReason = TEXT("[시스템] 초대 대상이 올바르지 않습니다.");
        return false;
    }

    if (MyPC == TargetPC)
    {
        OutFailReason = TEXT("[시스템] 자기 자신은 파티에 초대할 수 없습니다.");
        return false;
    }

    UPartyComponent* TargetPartyComp = TargetPC->GetPartyComponent();
    if (!TargetPartyComp)
    {
        TargetPartyComp = TargetPC->FindComponentByClass<UPartyComponent>();
    }
    if (!TargetPartyComp)
    {
        OutFailReason = TEXT("[시스템] 상대방의 파티 컴포넌트를 찾을 수 없습니다.");
        return false;
    }

    // 1. 이미 나와 같은 파티에 속해 있는지 검사
    if (IsInParty() && CurrentParty.GetMemberCount() > 1)
    {
        for (const FPartyMemberInfo& Member : CurrentParty.Members)
        {
            if (Member.PlayerController == TargetPC ||
                (!Member.Nickname.IsEmpty() && Member.Nickname.Equals(TargetPC->GetPlayerNickname(), ESearchCase::IgnoreCase)))
            {
                OutFailReason = FString::Printf(TEXT("[시스템] '%s' 님은 이미 현재 파티에 속해 있습니다."), *TargetPC->GetPlayerNickname());
                return false;
            }
        }
    }

    // 2. 상대방이 다른 파티에 속해 있는지 검사 (2인 이상 정상 파티인 경우)
    if (TargetPartyComp->IsInParty() && TargetPartyComp->CurrentParty.GetMemberCount() > 1)
    {
        OutFailReason = FString::Printf(TEXT("[시스템] '%s' 님은 이미 다른 파티에 속해 있습니다."), *TargetPC->GetPlayerNickname());
        return false;
    }

    // 3. 내 파티 권한 검사 (2인 이상 파티에서는 파티장만 초대 가능)
    if (IsInParty() && CurrentParty.GetMemberCount() > 1)
    {
        if (!IsPartyLeader())
        {
            OutFailReason = TEXT("[시스템] 파티장만 새로운 멤버를 초대할 수 있습니다.");
            return false;
        }

        // 4. 파티 정원 초과 검사
        if (CurrentParty.GetMemberCount() >= MaxPartyMembers)
        {
            OutFailReason = FString::Printf(TEXT("[시스템] 파티 인원이 가득 찼습니다. (최대 %d인)"), MaxPartyMembers);
            return false;
        }
    }

    return true;
}

void UPartyComponent::InvitePartyInternal(ANonPlayerController* TargetPC)
{
    ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwner());
    if (!MyPC || !TargetPC || MyPC == TargetPC) return;

    FString FailReason;
    if (!CanInvite(TargetPC, FailReason))
    {
        MyPC->Client_AddSystemMessage(FailReason);
        Client_PartyInviteDeclined(FailReason);
        return;
    }

    UPartyComponent* TargetPartyComp = TargetPC->GetPartyComponent();
    if (!TargetPartyComp)
    {
        TargetPartyComp = TargetPC->FindComponentByClass<UPartyComponent>();
    }
    if (!TargetPartyComp) return;

    // 상대방 파티에 본인 혼자만 남아있는 잔여 1인 파티인 경우 초기화
    if (TargetPartyComp->IsInParty() && TargetPartyComp->CurrentParty.GetMemberCount() <= 1)
    {
        TargetPartyComp->CurrentParty = FPartyData();
    }

    // 내가 아직 2인 이상 정식 파티가 아니라면 1인 파티로 초기화 및 생성
    if (!CurrentParty.IsInParty() || CurrentParty.GetMemberCount() <= 1)
    {
        CurrentParty.Members.Empty();
        CurrentParty.PartyId = FGuid::NewGuid();
        CurrentParty.LeaderNickname = MyPC->GetPlayerNickname();

        FPartyMemberInfo LeaderInfo;
        LeaderInfo.PlayerController = MyPC;
        LeaderInfo.Character = Cast<ANonCharacterBase>(MyPC->GetPawn());
        LeaderInfo.Nickname = MyPC->GetPlayerNickname();
        LeaderInfo.bIsLeader = true;
        
        if (LeaderInfo.Character.IsValid())
        {
            LeaderInfo.JobClass = LeaderInfo.Character->DefaultJobClass;
            if (const UNonAttributeSet* AS = LeaderInfo.Character->GetAttributeSet())
            {
                LeaderInfo.CurrentHP = AS->GetHP();
                LeaderInfo.MaxHP = AS->GetMaxHP();
                LeaderInfo.CurrentMP = AS->GetMP();
                LeaderInfo.MaxMP = AS->GetMaxMP();
            }
        }

        CurrentParty.Members.Add(LeaderInfo);
        Client_SyncPartyData(CurrentParty);
    }

    // 상대방에게 초대 알림 전송 (컴포넌트 및 컨트롤러 양쪽에 등록)
    TargetPartyComp->PendingInviterPC = MyPC;
    TargetPC->PendingPartyInviterPC = MyPC;

    // UI 및 이벤트 브로드캐스트
    TargetPartyComp->OnPartyInviteReceived.Broadcast(MyPC->GetPlayerNickname(), MyPC);
}

void UPartyComponent::Client_ReceivePartyInvite_Implementation(const FString& InviterNickname, ANonPlayerController* InviterPC)
{
    PendingInviterPC = InviterPC;
    OnPartyInviteReceived.Broadcast(InviterNickname, InviterPC);
}

void UPartyComponent::RespondToPartyInvite(bool bAccept)
{
    Server_RespondPartyInvite(bAccept);
}

void UPartyComponent::SetPendingInviterPC(ANonPlayerController* InPC)
{
    PendingInviterPC = InPC;
}

bool UPartyComponent::Server_RespondPartyInvite_Validate(bool bAccept)
{
    return true;
}

void UPartyComponent::Server_RespondPartyInvite_Implementation(bool bAccept)
{
    ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwner());
    if (!MyPC) return;

    if (!PendingInviterPC.IsValid())
    {
        PendingInviterPC = MyPC->PendingPartyInviterPC;
    }

    if (!PendingInviterPC.IsValid())
    {
        MyPC->Client_AddSystemMessage(TEXT("[시스템] 유효하지 않거나 만료된 파티 초대입니다."));
        return;
    }

    UPartyComponent* InviterPartyComp = PendingInviterPC->FindComponentByClass<UPartyComponent>();
    if (!InviterPartyComp)
    {
        PendingInviterPC = nullptr;
        MyPC->PendingPartyInviterPC = nullptr;
        return;
    }

    if (!bAccept)
    {
        InviterPartyComp->Client_PartyInviteDeclined(MyPC->GetPlayerNickname());
        if (ANonPlayerController* InviterPC = Cast<ANonPlayerController>(InviterPartyComp->GetOwner()))
        {
            InviterPC->Client_AddSystemMessage(FString::Printf(TEXT("[시스템] %s 님이 파티 초대를 거절했습니다."), *MyPC->GetPlayerNickname()));
        }
        MyPC->Client_AddSystemMessage(TEXT("[시스템] 파티 초대를 거절했습니다."));
        PendingInviterPC = nullptr;
        MyPC->PendingPartyInviterPC = nullptr;
        return;
    }

    // 파티 수락 처리
    if (InviterPartyComp->CurrentParty.GetMemberCount() >= MaxPartyMembers)
    {
        Client_PartyInviteDeclined(TEXT("초대한 파티의 인원이 이미 가득 찼습니다."));
        MyPC->Client_AddSystemMessage(TEXT("[시스템] 파티 인원이 이미 가득 찼습니다."));
        PendingInviterPC = nullptr;
        MyPC->PendingPartyInviterPC = nullptr;
        return;
    }

    // 만약 초대자의 파티가 아직 유효하게 생성되지 않은 상태라면 안전하게 생성
    if (!InviterPartyComp->CurrentParty.IsInParty())
    {
        ANonPlayerController* InviterPC = PendingInviterPC.Get();
        InviterPartyComp->CurrentParty.PartyId = FGuid::NewGuid();
        InviterPartyComp->CurrentParty.LeaderNickname = InviterPC->GetPlayerNickname();

        FPartyMemberInfo LeaderInfo;
        LeaderInfo.PlayerController = InviterPC;
        LeaderInfo.Character = Cast<ANonCharacterBase>(InviterPC->GetPawn());
        LeaderInfo.Nickname = InviterPC->GetPlayerNickname();
        LeaderInfo.bIsLeader = true;
        if (LeaderInfo.Character.IsValid())
        {
            LeaderInfo.JobClass = LeaderInfo.Character->DefaultJobClass;
            if (const UNonAttributeSet* AS = LeaderInfo.Character->GetAttributeSet())
            {
                LeaderInfo.CurrentHP = AS->GetHP();
                LeaderInfo.MaxHP = AS->GetMaxHP();
                LeaderInfo.CurrentMP = AS->GetMP();
                LeaderInfo.MaxMP = AS->GetMaxMP();
            }
        }
        InviterPartyComp->CurrentParty.Members.Add(LeaderInfo);
    }

    // 새 멤버 정보 생성
    FPartyMemberInfo NewMember;
    NewMember.PlayerController = MyPC;
    NewMember.Character = Cast<ANonCharacterBase>(MyPC->GetPawn());
    NewMember.Nickname = MyPC->GetPlayerNickname();
    NewMember.bIsLeader = false;

    if (NewMember.Character.IsValid())
    {
        NewMember.JobClass = NewMember.Character->DefaultJobClass;
        if (const UNonAttributeSet* AS = NewMember.Character->GetAttributeSet())
        {
            NewMember.CurrentHP = AS->GetHP();
            NewMember.MaxHP = AS->GetMaxHP();
            NewMember.CurrentMP = AS->GetMP();
            NewMember.MaxMP = AS->GetMaxMP();
        }
    }

    InviterPartyComp->CurrentParty.Members.Add(NewMember);

    // 파티에 속한 모든 멤버에게 갱신된 파티 데이터 배포 및 시스템 메시지 알림
    for (const FPartyMemberInfo& Member : InviterPartyComp->CurrentParty.Members)
    {
        if (Member.PlayerController.IsValid())
        {
            if (UPartyComponent* MemberPartyComp = Member.PlayerController->FindComponentByClass<UPartyComponent>())
            {
                MemberPartyComp->CurrentParty = InviterPartyComp->CurrentParty;
                MemberPartyComp->Client_SyncPartyData(InviterPartyComp->CurrentParty);
            }
            Member.PlayerController->Client_AddSystemMessage(FString::Printf(TEXT("[시스템] %s 님이 파티에 참가했습니다."), *NewMember.Nickname));
        }
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
            FString::Printf(TEXT("[서버] 파티 결성 완료! 파티원 %d/4명 (파티장: %s)"),
                InviterPartyComp->CurrentParty.GetMemberCount(),
                *InviterPartyComp->CurrentParty.LeaderNickname));
    }

    PendingInviterPC = nullptr;
    MyPC->PendingPartyInviterPC = nullptr;
}

void UPartyComponent::Client_PartyInviteDeclined_Implementation(const FString& TargetNickname)
{
    OnPartyInviteDeclined.Broadcast(TargetNickname);
}

void UPartyComponent::LeaveParty()
{
    Server_LeaveParty();
}

bool UPartyComponent::Server_LeaveParty_Validate()
{
    return true;
}

void UPartyComponent::Server_LeaveParty_Implementation()
{
    ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwner());
    if (!MyPC || !CurrentParty.IsInParty()) return;

    FString LeavingNickname = MyPC->GetPlayerNickname();
    int32 RemoveIdx = INDEX_NONE;

    for (int32 i = 0; i < CurrentParty.Members.Num(); ++i)
    {
        if (CurrentParty.Members[i].Nickname.Equals(LeavingNickname))
        {
            RemoveIdx = i;
            break;
        }
    }

    if (RemoveIdx != INDEX_NONE)
    {
        CurrentParty.Members.RemoveAt(RemoveIdx);
    }

    // 나 자신은 파티 해제
    FPartyData OldParty = CurrentParty;
    CurrentParty = FPartyData();
    Client_PartyDisbanded();
    MyPC->Client_AddSystemMessage(TEXT("[시스템] 파티에서 탈퇴했습니다."));

    // 남은 인원이 1명 이하인 경우 파티 자동 해산
    if (OldParty.Members.Num() <= 1)
    {
        for (const FPartyMemberInfo& Member : OldParty.Members)
        {
            if (Member.PlayerController.IsValid())
            {
                if (UPartyComponent* Comp = Member.PlayerController->FindComponentByClass<UPartyComponent>())
                {
                    Comp->CurrentParty = FPartyData();
                    Comp->Client_PartyDisbanded();
                }
                Member.PlayerController->Client_AddSystemMessage(TEXT("[시스템] 파티원이 부족하여 파티가 자동으로 해산되었습니다."));
            }
        }
    }
    else
    {
        // 파티장이 나갔다면 다음 사람에게 리더 위임
        if (OldParty.LeaderNickname.Equals(LeavingNickname) && OldParty.Members.Num() > 0)
        {
            OldParty.LeaderNickname = OldParty.Members[0].Nickname;
            OldParty.Members[0].bIsLeader = true;
        }

        // 남은 멤버들에게 갱신된 파티 데이터 전파 및 알림
        for (const FPartyMemberInfo& Member : OldParty.Members)
        {
            if (Member.PlayerController.IsValid())
            {
                if (UPartyComponent* Comp = Member.PlayerController->FindComponentByClass<UPartyComponent>())
                {
                    Comp->CurrentParty = OldParty;
                    Comp->Client_SyncPartyData(OldParty);
                }
                Member.PlayerController->Client_AddSystemMessage(FString::Printf(TEXT("[시스템] %s 님이 파티에서 탈퇴했습니다."), *LeavingNickname));
            }
        }
    }
}

void UPartyComponent::KickPartyMember(const FString& TargetNickname)
{
    Server_KickPartyMember(TargetNickname);
}

bool UPartyComponent::Server_KickPartyMember_Validate(const FString& TargetNickname)
{
    return true;
}

void UPartyComponent::Server_KickPartyMember_Implementation(const FString& TargetNickname)
{
    if (!IsPartyLeader() || TargetNickname.IsEmpty()) return;

    int32 KickIdx = INDEX_NONE;
    TWeakObjectPtr<ANonPlayerController> KickedPC = nullptr;

    for (int32 i = 0; i < CurrentParty.Members.Num(); ++i)
    {
        if (CurrentParty.Members[i].Nickname.Equals(TargetNickname))
        {
            KickIdx = i;
            KickedPC = CurrentParty.Members[i].PlayerController;
            break;
        }
    }

    if (KickIdx == INDEX_NONE) return;

    CurrentParty.Members.RemoveAt(KickIdx);

    // 추방된 플레이어 처리
    if (KickedPC.IsValid())
    {
        if (UPartyComponent* KickedComp = KickedPC->FindComponentByClass<UPartyComponent>())
        {
            KickedComp->CurrentParty = FPartyData();
            KickedComp->Client_PartyDisbanded();
        }
    }

    // 남은 멤버들에게 전파
    if (CurrentParty.Members.Num() <= 1)
    {
        for (const FPartyMemberInfo& Member : CurrentParty.Members)
        {
            if (Member.PlayerController.IsValid())
            {
                if (UPartyComponent* Comp = Member.PlayerController->FindComponentByClass<UPartyComponent>())
                {
                    Comp->CurrentParty = FPartyData();
                    Comp->Client_PartyDisbanded();
                }
            }
        }
    }
    else
    {
        for (const FPartyMemberInfo& Member : CurrentParty.Members)
        {
            if (Member.PlayerController.IsValid())
            {
                if (UPartyComponent* Comp = Member.PlayerController->FindComponentByClass<UPartyComponent>())
                {
                    Comp->CurrentParty = CurrentParty;
                    Comp->Client_SyncPartyData(CurrentParty);
                }
            }
        }
    }
}

void UPartyComponent::Client_SyncPartyData_Implementation(const FPartyData& NewPartyData)
{
    CurrentParty = NewPartyData;
    OnPartyUpdated.Broadcast(CurrentParty);
}

void UPartyComponent::Client_PartyDisbanded_Implementation()
{
    CurrentParty = FPartyData();
    OnPartyLeft.Broadcast();
}

void UPartyComponent::Server_SyncMyStats_Implementation(float CurrentHP, float MaxHP, float CurrentMP, float MaxMP)
{
    ANonPlayerController* MyPC = Cast<ANonPlayerController>(GetOwner());
    if (!MyPC || !CurrentParty.IsInParty()) return;

    bool bUpdated = false;
    for (FPartyMemberInfo& Member : CurrentParty.Members)
    {
        if (Member.Nickname.Equals(MyPC->GetPlayerNickname()))
        {
            Member.CurrentHP = CurrentHP;
            Member.MaxHP = MaxHP;
            Member.CurrentMP = CurrentMP;
            Member.MaxMP = MaxMP;
            bUpdated = true;
            break;
        }
    }

    if (bUpdated)
    {
        for (const FPartyMemberInfo& Member : CurrentParty.Members)
        {
            if (Member.PlayerController.IsValid())
            {
                if (UPartyComponent* Comp = Member.PlayerController->FindComponentByClass<UPartyComponent>())
                {
                    Comp->CurrentParty = CurrentParty;
                    Comp->Client_SyncPartyData(CurrentParty);
                }
            }
        }
    }
}
