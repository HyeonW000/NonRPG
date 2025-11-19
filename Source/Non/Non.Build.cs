// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Non : ModuleRules
{
	public Non(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags", "GameplayAbilities", "GameplayTasks",
			"AIModule", "UMG", "Slate", "SlateCore", "Niagara", "AnimGraphRuntime", "NavigationSystem", "NetCore" });
	}
}
