#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillTypes.h"
#include "PartyTypes.generated.h"

class ANonPlayerController;
class ANonCharacterBase;

/** 파티원 개별 상태 정보 구조체 */
USTRUCT(BlueprintType)
struct NON_API FPartyMemberInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Party")
    TWeakObjectPtr<ANonPlayerController> PlayerController = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Party")
    TWeakObjectPtr<ANonCharacterBase> Character = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Party")
    FString Nickname;

    UPROPERTY(BlueprintReadOnly, Category = "Party")
    EJobClass JobClass = EJobClass::Defender;

    UPROPERTY(BlueprintReadOnly, Category = "Party")
    int32 Level = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Party")
    float CurrentHP = 100.f;

    UPROPERTY(BlueprintReadOnly, Category = "Party")
    float MaxHP = 100.f;

    UPROPERTY(BlueprintReadOnly, Category = "Party")
    float CurrentMP = 50.f;

    UPROPERTY(BlueprintReadOnly, Category = "Party")
    float MaxMP = 50.f;

    UPROPERTY(BlueprintReadOnly, Category = "Party")
    bool bIsLeader = false;

    bool IsValidMember() const
    {
        return !Nickname.IsEmpty();
    }
};

/** 전체 파티 데이터 구조체 */
USTRUCT(BlueprintType)
struct NON_API FPartyData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Party")
    FGuid PartyId;

    UPROPERTY(BlueprintReadOnly, Category = "Party")
    FString LeaderNickname;

    UPROPERTY(BlueprintReadOnly, Category = "Party")
    TArray<FPartyMemberInfo> Members;

    bool IsInParty() const
    {
        return PartyId.IsValid() && Members.Num() > 0;
    }

    int32 GetMemberCount() const
    {
        return Members.Num();
    }
};

// UI 연동용 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyUpdated, const FPartyData&, PartyData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPartyInviteReceived, const FString&, InviterNickname, ANonPlayerController*, InviterController);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPartyLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyInviteDeclined, const FString&, TargetNickname);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyMemberKicked, const FString&, KickedNickname);
