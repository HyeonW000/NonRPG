#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Skill/SkillTypes.h" // [Fix] EJobClass 인식을 위해 추가
#include "CharacterCreationWidget.generated.h"

class UEditableTextBox;
class UEditableTextBox;
class UButton;
class UBorder; // [Fix] Forward Declaration 추가
class ANonCharacterBase; // [New]

/**
 * 캐릭터 생성 화면 위젯 (C++ Backing Class)
 */
UCLASS()
class NON_API UCharacterCreationWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;

protected:
	// -- UI Bindings --
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> Input_Name;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Class_Defender; // [Rename] Warrior -> Defender

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Class_Berserker; // [New] for Index 1

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Class_Cleric;   // [Rename] Mage -> Cleric (Index 2)

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Create;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Cancel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Back;

	// [New] 직업 선택 강조용 테두리
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Class_Defender;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Class_Berserker;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Class_Cleric;

	// [New] 다음(세부 설정으로 이동) 버튼
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Next;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UWidgetSwitcher> Switcher_Main;

	// -- Functions --
	UFUNCTION()
	void OnClickDefender();

	UFUNCTION()
	void OnClickBerserker();

	UFUNCTION()
	void OnClickCleric();
	
	UFUNCTION()
	void OnClickNext(); // [New]
	
	UFUNCTION()
	void OnClickBack();

	UFUNCTION()
	void OnClickCreate();
	
	UFUNCTION()
	void OnClickCancel();

	// 선택된 직업
	int32 SelectedClassIndex = 0;

public:
	// 외부 설정용
	int32 TargetSlotIndex = 0;

	UPROPERTY()
	TObjectPtr<class UCharacterSelectWidget> ParentSelectWidget; // 취소/완료 시 되돌아갈 곳

	// [New] 3D 미리보기용 캐릭터 포인터
	UPROPERTY()
	TObjectPtr<class ANonCharacterBase> PreviewCharacter;

	// [New] 미리보기 모델 업데이트 함수
	void UpdatePreviewModel(EJobClass NewJob);

	// [New] 카메라 전환용
	UPROPERTY()
	TObjectPtr<AActor> PreviewCameraActor;
	
	UPROPERTY()
	TObjectPtr<AActor> OriginalViewTarget;
};
