#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LobbyPlayerController.generated.h"

/**
 * 로비(캐릭터 생성 화면) 전용 플레이어 컨트롤러
 * - 마우스 커서 표시
 * - UI 입력 모드 설정
 */
UCLASS()
class NON_API ALobbyPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	ALobbyPlayerController();

protected:
	virtual void BeginPlay() override;

public:
	// [New] 타이틀 화면 위젯 (Press Any Key)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUserWidget> TitleWidgetClass;

	// [New] 캐릭터 선택 화면 위젯
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUserWidget> CharacterSelectWidgetClass;

	// [New] 캐릭터 생성 화면 위젯
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUserWidget> CharacterCreationWidgetClass;

	// 현재 떠있는 위젯 관리
	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<class UUserWidget> CurrentWidget;

	// [New] 타이틀 화면에서 호출하면 캐릭터 선택창으로 전환 (BP에서 호출)
	UFUNCTION(BlueprintCallable, Category = "UI")
	void StartCharacterSelect();

	// [New] 캐릭터 선택창 -> 캐릭터 생성창으로 전환 (BP에서 호출)
	// [Fix] 슬롯 번호(0, 1, 2)를 전달받아야 제대로 저장할 수 있음
	UFUNCTION(BlueprintCallable, Category = "UI")
	void StartCharacterCreation(int32 SlotIndex = 0);

	// [New] 캐릭터 생성 완료 후 카메라 복구 및 UI 전환을 위해 호출 (BP에서 호출)
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnCharacterCreationFinished();

	// [New] 특정 태그를 가진 카메라로 시점 전환 (BP에서 호출 가능)
	UFUNCTION(BlueprintCallable, Category = "View")
	void SetViewToCameraWithTag(FName Tag, float BlendTime = 0.0f);

protected:
	// [New] 로비(타이틀/선택)에서 사용할 카메라 액터의 태그 (기본값: LobbyCamera)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "View")
	FName LobbyCameraTag = TEXT("LobbyCamera");

	// [New] 캐릭터 생성 시 사용할 카메라 액터의 태그 (기본값: CreationCamera)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "View")
	FName CreationCameraTag = TEXT("CreationCamera");

public:
    // [New] 게임 시작 요청 (서버 트래블)
    UFUNCTION(Server, Reliable, BlueprintCallable)
    void ServerStartGame(const FString& MapName);

private:
	// [Optimization] 매번 태그로 검색하지 않도록 캐싱해둘 변수
	UPROPERTY()
	TObjectPtr<AActor> CachedLobbyCameraActor;

	UPROPERTY()
	TObjectPtr<AActor> CachedCreationCameraActor;
};
