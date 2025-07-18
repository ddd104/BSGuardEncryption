// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BSContentCommand : ModuleRules
{
	public BSContentCommand(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "ContentBrowser" });
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Projects",
				"Slate",
				"SlateCore",
				"BSGuardCore",
				"StatusBar"
			}
			);

		//IsConsoleApplication = true;
	}
}
