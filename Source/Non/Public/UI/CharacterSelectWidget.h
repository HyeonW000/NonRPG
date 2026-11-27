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
class UBorder; // [New]

/**
 * 캐릭터 선택(로비 메인) 위젯
 * - 캐릭터는 하나만 쓴다 (2026-11-28 결정, NON-DIS-14). 슬롯 0만 보이고 1·2는 숨김
 * - 나중에 타이틀 화면([계속하기] [방 만들기] [참가하기] ...)으로 바꿀 예정
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

	// [New] 선택 강조용 테두리
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Slot_0;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Slot_1;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Slot_2;
	
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

	/** 쓰는 캐릭터 칸 수 (한 캐릭터만). WBP에 남아 있는 칸 1·2는 숨긴다 */
	static constexpr int32 NumCharacterSlots = 1;

	// -- Functions --
	UFUNCTION() void OnClickSlot0();
	UFUNCTION() void OnClickSlot1();
	UFUNCTION() void OnClickSlot2();
	UFUNCTION() void OnClickStartGame();
	UFUNCTION() void OnClickDelete(); // [New]

	void HandleSlotClick(int32 SlotIndex);
	void RefreshLobbyCharacters(); // [New]
};
