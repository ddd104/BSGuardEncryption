//=============================================================
// Filename:       BSGuardEditor.h
// Publisher:      BigStar
// Creation Date:  2025-09-08
// Last Modified:  2025-09-08
// Version:        v1.0
//
// Description:
// Implement license-related verification functions
//=============================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

/**
 * 
 */
class BSGUARDCORE_API FBSGuardSettings
{
public:
	FBSGuardSettings(){}
	FString LicenseFilePath;
	
	FString KeyExpiration;

	// 验证密钥有效性（正确性及未过期）。成功返回true并将内部密钥设置好，失败返回false。
	bool ValidateAndSetKey();
    
	// 获取内部使用的原始AES对称密钥字节（32字节），如果密钥有效
	const TArray<uint8>& GetKeyBytes() const;

	// 检查密钥是否已验证通过且未过期
	bool IsKeyValid() const { return bKeyIsValid; }

private:
	// 内存中存储的实际AES密钥字节
	TArray<uint8> KeyBytes;
	bool bKeyIsValid = false;
};
