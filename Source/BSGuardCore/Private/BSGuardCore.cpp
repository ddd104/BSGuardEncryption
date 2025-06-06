// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSGuardCore.h"

#include "BSGuardCrypto.h"
#include "BSGuardPlatformFile.h"
#include "BSGuardSettings.h"
#include "BSCommonDefinition.h"

DEFINE_LOG_CATEGORY(LogBSGE);

#define LOCTEXT_NAMESPACE "FBSGuardCoreModule"

void FBSGuardCoreModule::StartupModule()
{
	UBSGuardSettings* Settings = GetMutableDefault<UBSGuardSettings>();
	if (Settings)
	{
                if (Settings->ValidateAndSetKey())
                {
                        // 将验证后的密钥字节设置给加密模块
                        FBSGuardCrypto::SetKey(Settings->GetKeyBytes(), Settings->GetUserId());
                }
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GuardEncryption: No valid key provided. Encryption will remain disabled."));
		}
	}

	// 保存原始平台文件指针
	OriginalPlatformFile = &FPlatformFileManager::Get().GetPlatformFile();
	// 创建我们的自定义平台文件并初始化
	GuardPlatformFile = MakeUnique<FBSGuardPlatformFile>();
	GuardPlatformFile->Initialize(OriginalPlatformFile, TEXT("BSGuardPlatformFile"));
	// 用GuardPlatformFile替换当前平台文件，使其成为顶层
	FPlatformFileManager::Get().SetPlatformFile(*GuardPlatformFile);

	UE_LOG(LogTemp, Log, TEXT("GuardEncryption: Custom platform file installed. Encryption %s."), 
		   FBSGuardCrypto::HasValidKey() ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void FBSGuardCoreModule::ShutdownModule()
{
	// 模块卸载时恢复原始PlatformFile，确保热重载不会叠加包裹
	if (OriginalPlatformFile)
	{
		FPlatformFileManager::Get().SetPlatformFile(*OriginalPlatformFile);
	}
	GuardPlatformFile.Reset();
	UE_LOG(LogTemp, Log, TEXT("GuardEncryption: Platform file restored to original on shutdown."));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBSGuardCoreModule, BSGuardCore)