// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class BSGuardEditor : ModuleRules
{
	public BSGuardEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "ContentBrowserData" });
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		OptimizeCode = CodeOptimization.Never;
		//bUsePrecompiled = true;
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"EditorStyle", 
				"UnrealEd",
				"ContentBrowser", 
				"AssetTools",
				"Projects", 
				"InputCore",
				"BSGuardCore",
				"AssetRegistry",
				"ToolMenus"
				//////////

			}
			);
		/*string EnginePath = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.AddRange(new[] {
			Path.Combine(EnginePath, "Source", "Editor", "ContentBrowser", "Private")
		});*/
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
			PublicDefinitions.Add("WITH_OPENSSL=1");
		}
	}
}
