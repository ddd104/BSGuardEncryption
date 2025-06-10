// Fill out your copyright notice in the Description page of Project Settings.


#include "BSGuardEncryptCmdlet.h"

#include "BSGuardCrypto.h"
#include "BSGuardSettings.h"

UBSGuardEncryptCmdlet::UBSGuardEncryptCmdlet()
{
	IsServer = false;
	LogToConsole = true;
}

UBSGuardEncryptCmdlet::~UBSGuardEncryptCmdlet()
{
	
}

int32 UBSGuardEncryptCmdlet::Main(const FString& Params)
{
	UE_LOG(LogTemp, Display, TEXT("GuardEncryptCmdlet started with params: %s"), *Params);

	// 在命令行中，无法通过UI输入密钥，因此需通过Params传入或在运行前已配置
	// 这里假设Params包含了密钥或在DefaultEngine.ini里存了密钥（实际不推荐明文ini）
	BSGuardSettings = MakeShared<FBSGuardSettings>();
	if (!BSGuardSettings || !BSGuardSettings->ValidateAndSetKey())
	{
		UE_LOG(LogTemp, Error, TEXT("No valid encryption key provided. Use -Key=<Base64Key> in Params."));
		return -1;
	}
	FBSGuardCrypto::SetKey(BSGuardSettings->GetKeyBytes());

	// 确定扫描路径，默认为项目Content目录
	FString TargetPath = FPaths::ProjectContentDir();
	// 可解析Params中自定义路径参数，这里简化假设用户未指定则全项目扫描

	// 收集所有uasset文件
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *TargetPath, TEXT("*.uasset"), true, false);
	int32 EncryptedCount = 0;
	for (FString& FilePath : Files)
	{
		// 检查对应资产是否已加密
		if (FBSGuardCrypto::IsEncryptedAssetFile(FilePath))
		{
			UE_LOG(LogTemp, Display, TEXT("File already encrypted, skip: %s"), *FilePath);
			continue;
		}
		// 加密主uasset文件
		if (FBSGuardCrypto::EncryptFile(FilePath))
		{
			// 如果有同名.uexp, .ubulk，一并加密
			FString UexpPath = FPaths::ChangeExtension(FilePath, TEXT(".uexp"));
			if (IFileManager::Get().FileExists(*UexpPath))
			{
				FBSGuardCrypto::EncryptFile(UexpPath);
			}
			FString UbulkPath = FPaths::ChangeExtension(FilePath, TEXT(".ubulk"));
			if (IFileManager::Get().FileExists(*UbulkPath))
			{
				FBSGuardCrypto::EncryptFile(UbulkPath);
			}
			UE_LOG(LogTemp, Display, TEXT("Encrypted: %s"), *FilePath);
			EncryptedCount++;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to encrypt: %s"), *FilePath);
		}
	}

	UE_LOG(LogTemp, Display, TEXT("GuardEncryptCmdlet completed. Total encrypted files: %d"), EncryptedCount);
	return 0;
}
