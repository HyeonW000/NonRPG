#include "Combat/ANS_ComboWindow.h"
#include "GameFramework/Actor.h"
#include "Character/NonCharacterBase.h"

void UANS_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp) return;

	if (AActor* Owner = MeshComp->GetOwner())
	{
		if (ANonCharacterBase* Character = Cast<ANonCharacterBase>(Owner))
		{
			Character->SetComboWindowOpen(true);
		}
	}
}

void UANS_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp) return;

	if (AActor* Owner = MeshComp->GetOwner())
	{
		if (ANonCharacterBase* Character = Cast<ANonCharacterBase>(Owner))
		{
			Character->SetComboWindowOpen(false);
		}
	}
}
