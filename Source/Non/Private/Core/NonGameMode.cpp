// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/NonGameMode.h"
#include "Character/NonCharacterBase.h"
#include "UObject/ConstructorHelpers.h"


ANonGameMode::ANonGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/Non/Blueprints/Character/BP_NonCharacterBase"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

