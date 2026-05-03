#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/NonInteractableInterface.h"
#include "NPCCharacter.generated.h"

// NPC의 역할(기능)을 정의하는 열거형
UENUM(BlueprintType)
enum class ENPCRole : uint8
{
    General     UMETA(DisplayName = "일반 (대화 전용)"),
    Merchant    UMETA(DisplayName = "상인 (물건 매매)"),
    Blacksmith  UMETA(DisplayName = "대장장이 (장비 강화)"),
    QuestGiver  UMETA(DisplayName = "퀘스트 주선자")
};

UCLASS()
class NON_API ANPCCharacter : public ACharacter, public INonInteractableInterface
{
    GENERATED_BODY()

public:
    ANPCCharacter();

protected:
    virtual void BeginPlay() override;

public:    
    virtual void Tick(float DeltaTime) override;

    // 상호작용 범위 (플레이어가 이 반경 안에 들어와야 F키 활성화)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
    class USphereComponent* InteractCollision;

    // NPC 머리 위에 이름을 띄워줄 위젯 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    class UWidgetComponent* NameWidget;

    // INonInteractableInterface 구현부
    virtual void Interact_Implementation(class ANonCharacterBase* Interactor) override;
    virtual FText GetInteractLabel_Implementation() override;
    virtual void SetInteractHighlight_Implementation(bool bEnable) override;

    // 대화나 상호작용 시 플레이어 쪽으로 몸을 돌리는 함수
    UFUNCTION(BlueprintCallable, Category = "NPC")
    void FaceInteractor(AActor* Interactor);

    // 대화 시 부드러운 회전 보간용 변수들
    bool bIsRotatingToPlayer = false;
    FRotator TargetRotation;
    
    UPROPERTY(EditAnywhere, Category = "NPC|Interaction")
    float RotationInterpSpeed = 5.0f;

    // 에디터에서 설정할 NPC 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    FText NPCName;

    // 기본 상호작용 텍스트 (예: "대화하기", "상점 열기")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    FText DefaultInteractLabel;

    // NPC의 역할 (상인, 대장장이 등)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    ENPCRole NPCRole = ENPCRole::General;

    // 이름표가 보이기 시작하는 최대 거리 (기본값: 1500)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Nameplate")
    float NameVisibilityDistance = 1500.f;

    // 이름표가 나타나고 사라지는 속도 (높을수록 빨리 사라짐, 기본값: 10.0)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Nameplate")
    float NameFadeSpeed = 10.f;

    // [New] 이 NPC 전용 대화 대본 (데이터 테이블)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Dialogue")
    class UDataTable* DialogueTable;

    // [New] 상호작용 시 대본에서 처음 시작할 대화의 Row Name (ID)
    // 비워두면 임시 인사말만 출력됩니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Dialogue")
    FName StartingDialogueID;
};
