#include "Combat/ANS_ComboWindow.h"
#include "GameFramework/Actor.h"
#include "Character/NonCharacterBase.h"
#include "Skill/SkillManagerComponent.h"

void UANS_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp) return;

	if (AActor* Owner = MeshComp->GetOwner())
	{
		// 1) 평타 콤보 윈도우 오픈
		if (ANonCharacterBase* Character = Cast<ANonCharacterBase>(Owner))
		{
			Character->SetComboWindowOpen(true);
		}

		// 2) 스킬 연계 콤보 윈도우 오픈 (정확한 노티파이 타이밍에 B 연계 스킬 활성화!)
		if (USkillManagerComponent* SkillMgr = Owner->FindComponentByClass<USkillManagerComponent>())
		{
			SkillMgr->OpenPendingComboWindow();
		}
	}
}

void UANS_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp) return;

	if (AActor* Owner = MeshComp->GetOwner())
	{
		// 1) 평타 콤보 윈도우 닫기
		if (ANonCharacterBase* Character = Cast<ANonCharacterBase>(Owner))
		{
			Character->SetComboWindowOpen(false);
		}

		// 2) 스킬 연계 대기 종료
		if (USkillManagerComponent* SkillMgr = Owner->FindComponentByClass<USkillManagerComponent>())
		{
			SkillMgr->ClosePendingComboWindow();
		}
	}
}
