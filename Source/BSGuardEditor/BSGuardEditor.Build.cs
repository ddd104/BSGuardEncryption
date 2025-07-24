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
				"UnrealEd",    // 编辑器样式和UnrealEd功能
				"ContentBrowser", 
				"AssetTools",
				"Projects", 
				"InputCore",      // 可能用到输入/图标
				"BSGuardCore",
			}
			);
		/*string EnginePath = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.AddRange(new[] {
			Path.Combine(EnginePath, "Source", "Editor", "ContentBrowser", "Private")
		});*/
	}
}
