// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSGuardCore.h"

#include "BSGuardCrypto.h"
#include "BSGuardPlatformFile.h"
#include "BSGuardSettings.h"

#define LOCTEXT_NAMESPACE "FBSGuardCoreModule"



struct FBSGE_EarlyInstaller
{
	FBSGE_EarlyInstaller()
	{
		UE_LOG(LogTemp, Display, TEXT("FBSGE_EarlyInstaller"));
		// 保存原始平台文件指针
		OriginalPlatformFile = &FPlatformFileManager::Get().GetPlatformFile();
		// 创建我们的自定义平台文件并初始化
		GuardPlatformFile = MakeUnique<FBSGuardPlatformFile>();
		GuardPlatformFile->Initialize(OriginalPlatformFile, TEXT("BSGuardPlatformFile"));
		// 用GuardPlatformFile替换当前平台文件，使其成为顶层
		FPlatformFileManager::Get().SetPlatformFile(*GuardPlatformFile);


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
		UE_LOG(LogTemp, Log, TEXT("GuardEncryption: Custom platform file installed. Encryption %s."), 
			   FBSGuardCrypto::HasValidKey() ? TEXT("ENABLED") : TEXT("DISABLED"));
	}
	
public:
	IPlatformFile* OriginalPlatformFile;
	TUniquePtr<class FBSGuardPlatformFile> GuardPlatformFile;

	TSharedPtr<class FBSGuardSettings> BSGuardSettings;
};  


static FBSGE_EarlyInstaller BSGE_PlatformFileAutoInstaller;

IPlatformFile* FBSGuardCoreModule::GetPlatformFile()
{
	static FBSGuardPlatformFile Guard;
	return &Guard;
}

void FBSGuardCoreModule::StartupModule()
{
	UE_LOG(LogTemp, Warning, TEXT("BSGuardPF StartupModule BEGIN"));
}

void FBSGuardCoreModule::ShutdownModule()
{
	
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBSGuardCoreModule, BSGuardCore)