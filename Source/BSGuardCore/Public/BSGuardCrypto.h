#pragma once
#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "BSGuardCrypto.generated.h"


DECLARE_DELEGATE_RetVal(bool, FRetBoolDelegate);

#if WITH_EDITOR
	class FBSGE_AssetActions;
#endif

class FBSGuardCrypto
{
public:
	// 设置AES对称密钥（32字节）。通常从UGuardSettings验证后调用。
	static void SetKey(const TArray<uint8>& InKey);

	// 检查当前是否有有效密钥
	BSGUARDCORE_API static bool HasValidKey();

	// 判断文件是否是加密的资产文件
	BSGUARDCORE_API static bool IsEncryptedAssetFile(const FString& FilePath);
	
	// AES-GCM 加解密实现
	static bool Encrypt(const TArray<uint8>& PlainData, TArray<uint8>& OutEncryptedData);
	static bool Decrypt(const TArray<uint8>& EncryptedData, TArray<uint8>& OutPlainData);

	static bool GenRandomBytes(uint8* Out, int32 Num);
	static bool IsValidAction();
	BSGUARDCORE_API static bool ShouldEncryptAsset(const FString& FilePath);

	BSGUARDCORE_API static const FString& ChooseHeaderExt(const FAssetData& AD); 
private:
	static bool bKeyIsSet;
	// 当前使用的对称密钥和有效性
	static uint8 Key[32];
	// 对指定文件进行AES-GCM加密，输出加密文件（覆盖原文件或创建新的）。
	BSGUARDCORE_API static bool EncryptFile(const FString& FilePath);
	// 对指定文件进行AES-GCM解密，还原为明文文件（覆盖原文件内容）。
	BSGUARDCORE_API static bool DecryptFile(const FString& FilePath);

	BSGUARDCORE_API static FRetBoolDelegate bRetBoolDelegate;
#if WITH_EDITOR
	friend class FBSGE_AssetActions;
#endif
};




UCLASS()
class UBSGEncryptUserData : public UAssetUserData
{
	GENERATED_BODY()

	virtual void Serialize(FArchive& Ar) override;
};


namespace BSGEncrypt
{
	BSGUARDCORE_API  void MarkPackageEncrypted(UPackage* Pkg);

	BSGUARDCORE_API  void MarkPackagePlain(UPackage* Pkg);
	
	BSGUARDCORE_API  bool IsPackageEncrypted(UPackage* Pkg);

	BSGUARDCORE_API  bool IsObjectEncrypted(UObject* Obj);
}