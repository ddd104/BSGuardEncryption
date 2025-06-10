#include "BSLicenseUtils..h"
#include "BSCommonDefinition.h"
#include "Interfaces/IPluginManager.h"
#include "Serialization/MemoryReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Base64.h"



TArray<uint8> FBSLicenseUtils::SharedKey = TArray<uint8>();

FString FBSLicenseUtils::GetPublicKeyPem()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BSGuardEncryption"));
	if (!Plugin.IsValid())
	{
		return FString();
	}
	FString KeyPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("license"), TEXT("root_public.pem"));
	FString Pem;
	FFileHelper::LoadFileToString(Pem, *KeyPath);
	return Pem;
}

TArray<uint8> FBSLicenseUtils::GetSharedKey()
{
	if (SharedKey.Num() == 0)
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BSGuardEncryption"));
		if (!Plugin.IsValid())
		{
			return TArray<uint8>();
		}
		FString KeyPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("license"), TEXT("SharedKey"));
		FFileHelper::LoadFileToArray(SharedKey, *KeyPath);
	}
	
    return SharedKey;
}

#if WITH_OPENSSL
static bool VerifySignatureOpenSSL(const TArray<uint8>& Data, const TArray<uint8>& SigBytes)
{
	FString Pem = FBSLicenseUtils::GetPublicKeyPem();
	if (Pem.IsEmpty())
	{
		return false;
	}
	FTCHARToUTF8 PemUtf8(*Pem);
	BIO* Bio = BIO_new_mem_buf((void*)PemUtf8.Get(), PemUtf8.Length());
	if (!Bio)
	{
		return false;
	}
	EVP_PKEY* PKey = PEM_read_bio_PUBKEY(Bio, NULL, NULL, NULL);
	BIO_free(Bio);
	if (!PKey)
	{
		return false;
	}
	EVP_MD_CTX* Ctx = EVP_MD_CTX_new();
	if (!Ctx)
	{
		EVP_PKEY_free(PKey);
		return false;
	}
	bool bResult = false;
	if (EVP_DigestVerifyInit(Ctx, NULL, EVP_sha256(), NULL, PKey) == 1)
	{
		if (EVP_DigestVerifyUpdate(Ctx, Data.GetData(), Data.Num()) == 1)
		{
			bResult = (EVP_DigestVerifyFinal(Ctx, SigBytes.GetData(), SigBytes.Num()) == 1);
		}
	}
	EVP_MD_CTX_free(Ctx);
	EVP_PKEY_free(PKey);
	return bResult;
}
#endif

bool FBSLicenseUtils::LoadLicense(const FString& FilePath, FBSLicenseData& OutData)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read license file: %s"), *FilePath);
		return false;
	}


	FMemoryReader Ar(Bytes, true);
	ANSICHAR Magic[5] = {0};
	Ar.Serialize(Magic, 4);
	if (FCStringAnsi::Strncmp(Magic, "BSL1", 4) != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid license magic"));
		return false;
	}

	uint32 UserLen = 0;
	Ar << UserLen;
	TArray<char> UserBuf;
	UserBuf.SetNum(UserLen);
	Ar.Serialize(UserBuf.GetData(), UserLen);
	FString User = UTF8_TO_TCHAR(UserBuf.GetData());

	uint32 AssetLen = 0;
	Ar << AssetLen;
	TArray<char> AssetBuf;
	AssetBuf.SetNum(AssetLen);
	Ar.Serialize(AssetBuf.GetData(), AssetLen);
	FString Asset = UTF8_TO_TCHAR(AssetBuf.GetData());

	int64 ExpireUnix = 0;
	Ar << ExpireUnix;

	uint32 KeyLen = 0;
	Ar << KeyLen;
	TArray<uint8> KeyBytes;
	KeyBytes.SetNum(KeyLen);
	Ar.Serialize(KeyBytes.GetData(), KeyLen);

	uint32 SigLen = 0;
	Ar << SigLen;
	TArray<uint8> SigBytes;
	SigBytes.SetNum(SigLen);
	Ar.Serialize(SigBytes.GetData(), SigLen);

	int32 DataLen = Bytes.Num() - SigLen - sizeof(uint32);
	TArray<uint8> DataToVerify;
	DataToVerify.Append(Bytes.GetData(), DataLen);

#if WITH_OPENSSL
	if (!VerifySignatureOpenSSL(DataToVerify, SigBytes))
	{
		UE_LOG(LogTemp, Error, TEXT("License signature verification failed"));
		return false;
	}
#else
	UE_LOG(LogTemp, Warning, TEXT("OpenSSL not available, skipping signature check"));
#endif

	OutData.User = User;
	OutData.AssetPack = Asset;
	OutData.ExpireDate = FDateTime::FromUnixTimestamp(ExpireUnix).ToIso8601();
	OutData.SharedKey = FBase64::Encode(KeyBytes);
	return true;
}