// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class BSGuardCore : ModuleRules
{
	public string GetOpenSSLVersion()
	{
		string ret = "1.1.1t";
		if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 2)
		{
			ret = "1.1.1t";
		}
		return ret;
	}
	public BSGuardCore(ReadOnlyTargetRules Target) : base(Target)
	{
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
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicDefinitions.Add("WITH_OPENSSL=1");

			// 获取 OpenSSL 路径（确保你使用的版本号与本地 Engine/Source/ThirdParty/OpenSSL 对应）
			string OpenSSLVersion = GetOpenSSLVersion();
			string OpenSSLPath = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", OpenSSLVersion);
			string OpenSSLInclude = Path.Combine(OpenSSLPath, "include", "Android");

			PublicIncludePaths.Add(OpenSSLInclude);
			
			string archSubDir = "ARM64";
			if (Target.Architecture == UnrealArch.X64)
			{
				archSubDir = "x64";
			}
			PublicAdditionalLibraries.Clear(); // 清除上面添加的 ARMv7 的库
			PublicAdditionalLibraries.Add(Path.Combine(OpenSSLPath, "lib", "Android", archSubDir, "libssl.a"));
			PublicAdditionalLibraries.Add(Path.Combine(OpenSSLPath, "lib", "Android", archSubDir, "libcrypto.a"));
		}
		else
		{
			PublicDefinitions.Add("WITH_OPENSSL=0");
		}
		
		string PluginDir = ModuleDirectory;  // 或使用 Plugin->GetBaseDir()
		RuntimeDependencies.Add(Path.Combine(PluginDir, "../../license/License.license"));
		RuntimeDependencies.Add(Path.Combine(PluginDir, "../../license/root_public.pem"));
		RuntimeDependencies.Add(Path.Combine(PluginDir, "../../license/SharedKey"));
	}
}
