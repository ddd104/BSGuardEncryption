#include "BSLicenseUtils..h"
#include "BSCommonDefinition.h"
#include "Interfaces/IPluginManager.h"


static FString GetPublicKeyPem()
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

#if WITH_OPENSSL
static bool VerifySignatureOpenSSL(const FString& Data, const TArray<uint8>& SigBytes)
{
	FString Pem = GetPublicKeyPem();
	if (Pem.IsEmpty()) return false;
	BIO* Bio = BIO_new_mem_buf((void*)TCHAR_TO_ANSI(*Pem), Pem.Len());
	if (!Bio) return false;
	EVP_PKEY* PKey = PEM_read_bio_PUBKEY(Bio, NULL, NULL, NULL);
	BIO_free(Bio);
	if (!PKey) return false;
	EVP_MD_CTX* Ctx = EVP_MD_CTX_new();
	if (!Ctx)
	{
		EVP_PKEY_free(PKey);
		return false;
	}
	bool bResult = false;
	if (EVP_DigestVerifyInit(Ctx, NULL, EVP_sha256(), NULL, PKey) == 1)
	{
		FTCHARToUTF8 Converter(*Data);
		if (EVP_DigestVerifyUpdate(Ctx, Converter.Get(), Converter.Length()) == 1)
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
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read license file: %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("License JSON parse failed: %s"), *FilePath);
		return false;
	}

	FString Signature;
	if (!JsonObj->TryGetStringField(TEXT("Signature"), Signature))
	{
		UE_LOG(LogTemp, Error, TEXT("License missing Signature field"));
		return false;
	}
	JsonObj->RemoveField(TEXT("Signature"));

	FString Canonical;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Canonical);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);

	TArray<uint8> SigBytes;
	if (!FBase64::Decode(Signature, SigBytes))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to decode license signature"));
		return false;
	}

#if WITH_OPENSSL
	if (!VerifySignatureOpenSSL(Canonical, SigBytes))
	{
		UE_LOG(LogTemp, Error, TEXT("License signature verification failed"));
		return false;
	}
#else
	UE_LOG(LogTemp, Warning, TEXT("OpenSSL not available, skipping signature check"));
#endif

	JsonObj->TryGetStringField(TEXT("AssetPack"), OutData.AssetPack);
	JsonObj->TryGetStringField(TEXT("User"), OutData.User);
	JsonObj->TryGetStringField(TEXT("ExpireDate"), OutData.ExpireDate);
	JsonObj->TryGetStringField(TEXT("SharedKey"), OutData.SharedKey);
	return true;
}