// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class BSGuardCore : ModuleRules
{
	public BSGuardCore(ReadOnlyTargetRules Target) : base(Target)
	{
        // 需要链接到 Program
        if (Target.Type == TargetType.Program)
        {
            OptimizeCode = CodeOptimization.InShippingBuildsOnly;
        }
        // 让 UBT 对所有 Target 预编译
        PrecompileForTargets = PrecompileTargetsType.Any;
        
        bPrecompile = false;
        
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		OptimizeCode = CodeOptimization.Never;
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Projects",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Mac)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
			PublicDefinitions.Add("WITH_OPENSSL=1");
		}
		
		PublicDefinitions.Add("WITH_CanPackageWithoutLicense=1");
		PublicDefinitions.Add("WITH_CanPackagingWithEncryption=1");
	}
	
}
