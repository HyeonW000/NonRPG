#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Skill/SkillTypes.h" // [New] EJobClass 정의 포함
#include "CharacterSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UImage; // [New]
class UTexture2D; // [New]
class UCharacterCreationWidget;

/**
 * 캐릭터 선택(로비 메인) 위젯
 * - 슬롯 목록 표시
 * - 선택된 슬롯으로 게임 시작
 */
UCLASS()
class NON_API UCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;

	// UI 갱신 (저장된 데이터 확인)
	UFUNCTION(BlueprintCallable)
	void UpdateSlotDisplay();

protected:
	// -- UI Bindings --
	// 예시: 3개의 슬롯 버튼 (나중에 Grid패널로 동적 생성 가능하지만 일단 정적으로)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Slot_0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Slot_1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Slot_2;
	
	// 각 슬롯의 텍스트 (비어있음/+ or 이름)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Slot_0;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Slot_1;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Slot_2;

	// [New] 각 슬롯의 직업 아이콘 이미지
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UImage> Image_Slot_0;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UImage> Image_Slot_1;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UImage> Image_Slot_2;

	// [New] 직업별 아이콘 맵 (BP 설정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby")
	TMap<EJobClass, UTexture2D*> ClassIcons;

	// 게임 시작 버튼
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_StartGame;
	
	// [New] 캐릭터 삭제 버튼
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Delete;

	// 생성 위젯 클래스 (BP에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UCharacterCreationWidget> CreationWidgetClass;

	// -- Internal State --
	int32 SelectedSlotIndex = -1; // -1: 선택 안함

	// -- Functions --
	UFUNCTION() void OnClickSlot0();
	UFUNCTION() void OnClickSlot1();
	UFUNCTION() void OnClickSlot2();
	UFUNCTION() void OnClickStartGame();
	UFUNCTION() void OnClickDelete(); // [New]

	void HandleSlotClick(int32 SlotIndex);
	void RefreshLobbyCharacters(); // [New]
};
