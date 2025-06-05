#pragma once
#include "CoreMinimal.h"

class BSGUARDCORE_API FBSGuardCrypto
{
public:
	// 设置AES对称密钥（32字节）。通常从UGuardSettings验证后调用。
	static void SetKey(const TArray<uint8>& InKey);

	// 检查当前是否有有效密钥
	static bool HasValidKey();

	// 判断文件是否是加密的资产文件（通过魔数头判断）
	static bool IsEncryptedAssetFile(const FString& FilePath);

	// 对指定文件进行AES-GCM加密，输出加密文件（覆盖原文件或创建新的）。
	static bool EncryptFile(const FString& FilePath);

	// 对指定文件进行AES-GCM解密，还原为明文文件（覆盖原文件内容）。
	static bool DecryptFile(const FString& FilePath);


	// AES-GCM 加解密实现
	static bool Encrypt(const TArray<uint8>& PlainData, TArray<uint8>& OutEncryptedData, TArray<uint8>& OutIV, TArray<uint8>& OutAuthTag);
	static bool Decrypt(const TArray<uint8>& EncryptedData, const TArray<uint8>& IV, const TArray<uint8>& AuthTag, TArray<uint8>& OutPlainData);

	static bool GenRandomBytes(uint8* Out, int32 Num);
	// 当前使用的对称密钥和有效性
	static uint8 Key[32];
	static bool bKeyIsSet;
};
