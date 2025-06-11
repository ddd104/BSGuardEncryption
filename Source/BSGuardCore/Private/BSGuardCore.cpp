// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSGuardCore.h"

#include "BSGuardCrypto.h"
#include "BSGuardPlatformFile.h"
#include "BSGuardSettings.h"

#define LOCTEXT_NAMESPACE "FBSGuardCoreModule"


IPlatformFile* FBSGuardCoreModule::Base = nullptr;
IPlatformFile* FBSGuardCoreModule::OriginalPlatformFile = nullptr;
TUniquePtr<FBSGuardPlatformFile> FBSGuardCoreModule::GuardPlatformFile = nullptr;

struct FNTAG_EarlyInstaller
{
	FNTAG_EarlyInstaller()
	{
		FBSGuardCoreModule::Base = &FPlatformFileManager::Get().GetPlatformFile();
		// 保存原始平台文件指针
		FBSGuardCoreModule::OriginalPlatformFile = &FPlatformFileManager::Get().GetPlatformFile();
		// 创建我们的自定义平台文件并初始化
		FBSGuardCoreModule::GuardPlatformFile = MakeUnique<FBSGuardPlatformFile>();
		FBSGuardCoreModule::GuardPlatformFile->Initialize(FBSGuardCoreModule::OriginalPlatformFile, TEXT("BSGuardPlatformFile"));
		// 用GuardPlatformFile替换当前平台文件，使其成为顶层
		FPlatformFileManager::Get().SetPlatformFile(*FBSGuardCoreModule::GuardPlatformFile);

		UE_LOG(LogTemp, Log, TEXT("GuardEncryption: Custom platform file installed. Encryption %s."), 
			   FBSGuardCrypto::HasValidKey() ? TEXT("ENABLED") : TEXT("DISABLED"));
	}
};  



void FBSGuardCoreModule::StartupModule()
{
	BSGuardSettings = MakeShared<FBSGuardSettings>();
	if (BSGuardSettings)
	{
		if (BSGuardSettings->ValidateAndSetKey())
		{
			// 将验证后的密钥字节设置给加密模块
			FBSGuardCrypto::SetKey(BSGuardSettings->GetKeyBytes());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GuardEncryption: No valid key provided. Encryption will remain disabled."));
		}
	}
	
	static FNTAG_EarlyInstaller GNTAG_PlatformFileAutoInstaller;

}

void FBSGuardCoreModule::ShutdownModule()
{
	/*// 模块卸载时恢复原始PlatformFile，确保热重载不会叠加包裹
	if (OriginalPlatformFile)
	{
		FPlatformFileManager::Get().SetPlatformFile(*OriginalPlatformFile);
	}
	GuardPlatformFile.Reset();
	BSGuardSettings.Reset();
	UE_LOG(LogTemp, Log, TEXT("GuardEncryption: Platform file restored to original on shutdown."));*/
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBSGuardCoreModule, BSGuardCore)