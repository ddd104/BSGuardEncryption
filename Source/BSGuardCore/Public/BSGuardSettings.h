// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BSGuardSettings.generated.h"

/**
 * 
 */
UCLASS(Config=EditorPerProjectUserSettings)
class BSGUARDCORE_API UBSGuardSettings : public UObject
{
	GENERATED_BODY()
public:
	// 用户在编辑器中输入的密钥字符串（旧方式）或选择的License文件路径
	UPROPERTY(EditAnywhere, Category="GuardEncryption", meta=(DisplayName="Encryption Key or License"), Transient)
	FString UserKeyInput;

	// 新增：License文件路径，可在项目设置中选择
	UPROPERTY(EditAnywhere, Category="GuardEncryption", meta=(DisplayName="License File"))
	FString LicenseFilePath;

	// 密钥过期时间（例如GMT字符串或时间戳），可选由插件提供，只读显示
	UPROPERTY(EditAnywhere, Category="GuardEncryption", meta=(DisplayName="Key Expiration"), Transient)
	FString KeyExpiration;

	// 验证密钥有效性（正确性及未过期）。成功返回true并将内部密钥设置好，失败返回false。
	bool ValidateAndSetKey();
    
	// 获取内部使用的原始AES对称密钥字节（32字节），如果密钥有效
	const TArray<uint8>& GetKeyBytes() const { return KeyBytes; }

	// 检查密钥是否已验证通过且未过期
	bool IsKeyValid() const { return bKeyIsValid; }

private:
	// 内存中存储的实际AES密钥字节
	TArray<uint8> KeyBytes;
	bool bKeyIsValid = false;
};
