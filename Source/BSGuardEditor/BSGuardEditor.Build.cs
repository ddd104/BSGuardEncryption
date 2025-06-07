// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BSGuardEditor : ModuleRules
{
	public BSGuardEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		OptimizeCode = CodeOptimization.Never;
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
				"BSGuardCore"
			}
			);

	}
}
