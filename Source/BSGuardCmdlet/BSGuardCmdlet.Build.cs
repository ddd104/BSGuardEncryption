// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BSGuardCmdlet : ModuleRules
{
	public BSGuardCmdlet(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Projects", 
				"BSGuardCore"
			}
			);

		//IsConsoleApplication = true;
	}
}
