// Copyright (c) 2025 BigStar. All Rights Reserved.

#include "BSLicenseUtils.h"
#include "BSCommonDefinition.h"
#include "Interfaces/IPluginManager.h"
#include "Serialization/MemoryReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Base64.h"
#if WITH_OPENSSL
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#endif


TArray<uint8> FBSLicenseUtils::SharedKey = TArray<uint8>();

FString FBSLicenseUtils::GetPublicKeyPem()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BSGuardEncryption"));
	if (!Plugin.IsValid())
	{
		return FString();
	}
	FString KeyPath = FPaths::Combine(GetPythonPath(),TEXT("license"), TEXT("root_public.pem"));
	FString Pem;
	FFileHelper::LoadFileToString(Pem, *KeyPath);
	return Pem;
}

// BSK1 structure
struct FBSK1Header
{
	uint8 Mode = 0;
	TArray<uint8> Salt;      // s bytes
	TArray<uint8> Nonce;     // 12 bytes
	TArray<uint8> Cipher;    // ct_len bytes
	TArray<uint8> Tag;       // 16 bytes
};

static bool ParseBSK1(const TArray<uint8>& Data, FBSK1Header& Out)
{
	if (Data.Num() < 4 + 1 + 1 + 12 + 2 + 16)
		return false;

	int32 Off = 0;
	auto Read = [&](int32 Len) -> const uint8* {
		const uint8* P = Data.GetData() + Off;
		Off += Len;
		return P;
	};

	if (FMemory::Memcmp(Read(4), "BSK1", 4) != 0)
		return false;

	Out.Mode = *Read(1);
	if (Out.Mode != 1) // Only PUBKDF is supported
		return false;

	uint8 SaltLen = *Read(1);
	Out.Salt.SetNumUninitialized(SaltLen);
	FMemory::Memcpy(Out.Salt.GetData(), Read(SaltLen), SaltLen);

	Out.Nonce.SetNumUninitialized(12);
	FMemory::Memcpy(Out.Nonce.GetData(), Read(12), 12);

	uint16 CtLen = 0;
	FMemory::Memcpy(&CtLen, Read(2), 2);

	Out.Cipher.SetNumUninitialized(CtLen);
	FMemory::Memcpy(Out.Cipher.GetData(), Read(CtLen), CtLen);

	Out.Tag.SetNumUninitialized(16);
	FMemory::Memcpy(Out.Tag.GetData(), Read(16), 16);

	return true;
}


TArray<uint8> FBSLicenseUtils::GetSharedKey()
{
	TArray<uint8> Empty;

    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BSGuardEncryption"));
    if (!Plugin.IsValid())
        return Empty;

    FString EncPath = FPaths::Combine(GetPythonPath(),TEXT("license"), TEXT("SharedKey.enc"));
    FString PubPath = FPaths::Combine(GetPythonPath(),TEXT("license"), TEXT("root_public.pem"));

    TArray<uint8> EncBytes;
    if (!FFileHelper::LoadFileToArray(EncBytes, *EncPath))
        return Empty;

#if WITH_OPENSSL
    FBSK1Header H;
    if (!ParseBSK1(EncBytes, H))
        return Empty;

    // Read public key PEM -> EVP_PKEY
    FString PemPub;
    if (!FFileHelper::LoadFileToString(PemPub, *PubPath))
        return Empty;

    FTCHARToUTF8 PemUtf8(*PemPub);
    BIO* Bio = BIO_new_mem_buf((void*)PemUtf8.Get(), PemUtf8.Length());
    if (!Bio)
        return Empty;

    EVP_PKEY* PKey = PEM_read_bio_PUBKEY(Bio, NULL, NULL, NULL);
    BIO_free(Bio);
    if (!PKey)
        return Empty;

    // Export SPKI DER
    int SpkiLen = i2d_PUBKEY(PKey, NULL);
    if (SpkiLen <= 0) { EVP_PKEY_free(PKey); return Empty; }

    TArray<uint8> Spki;
    Spki.SetNumUninitialized(SpkiLen);
    unsigned char* P = Spki.GetData();
    if (i2d_PUBKEY(PKey, &P) != SpkiLen) { EVP_PKEY_free(PKey); return Empty; }
    EVP_PKEY_free(PKey);

    // key = SHA256( SPKI || salt )
    TArray<uint8> Key;
    Key.SetNumUninitialized(32);
    {
        EVP_MD_CTX* Ctx = EVP_MD_CTX_new();
        if (!Ctx) return Empty;
        if (!(EVP_DigestInit_ex(Ctx, EVP_sha256(), NULL) > 0 &&
              EVP_DigestUpdate(Ctx, Spki.GetData(), Spki.Num()) > 0 &&
              EVP_DigestUpdate(Ctx, H.Salt.GetData(), H.Salt.Num()) > 0))
        {
            EVP_MD_CTX_free(Ctx);
            return Empty;
        }
        unsigned int OutLen = 0;
        if (!(EVP_DigestFinal_ex(Ctx, Key.GetData(), &OutLen) > 0 && OutLen == 32))
        {
            EVP_MD_CTX_free(Ctx);
            return Empty;
        }
        EVP_MD_CTX_free(Ctx);
    }

    // AES-256-GCM decryption (AAD = "BSK1" + mode(=1) + salt)
    EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
    if (!Ctx) return Empty;

    bool bOK =
        EVP_DecryptInit_ex(Ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) > 0 &&
        EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_SET_IVLEN, H.Nonce.Num(), NULL) > 0 &&
        EVP_DecryptInit_ex(Ctx, NULL, NULL, Key.GetData(), H.Nonce.GetData()) > 0;

    // AAD
    const uint8 AADPrefix[5] = { 'B','S','K','1', 1 }; // mode = 1
    int Outl = 0;
    if (bOK) bOK = EVP_DecryptUpdate(Ctx, NULL, &Outl, AADPrefix, 5) > 0;
    if (bOK && H.Salt.Num() > 0) bOK = EVP_DecryptUpdate(Ctx, NULL, &Outl, H.Salt.GetData(), H.Salt.Num()) > 0;

    // Ciphertext
    int PlainLen = 0;
    TArray<uint8> Plain;
    Plain.SetNumUninitialized(H.Cipher.Num());
    if (bOK) bOK = EVP_DecryptUpdate(Ctx, Plain.GetData(), &PlainLen, H.Cipher.GetData(), H.Cipher.Num()) > 0;

    // Tag
    if (bOK) bOK = EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_SET_TAG, 16, (void*)H.Tag.GetData()) > 0;

    int FinalLen = 0;
    if (bOK) bOK = EVP_DecryptFinal_ex(Ctx, Plain.GetData() + PlainLen, &FinalLen) > 0;

    EVP_CIPHER_CTX_free(Ctx);

    if (!bOK)
        return Empty;

    Plain.SetNum(PlainLen + FinalLen);
    return Plain; // Expected 32 bytes
#else
    return Empty;
#endif
}

FString FBSLicenseUtils::GetPythonPath()
{
	FString EncPath = TEXT("");
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BSGuardEncryption"));
	if (!Plugin.IsValid())
	{
		return EncPath;
	}
	EncPath = FPaths::Combine(Plugin->GetContentDir(), TEXT("Python"), TEXT("Lib"),TEXT("site-packages"));
	return EncPath;
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
#if WITH_CanPackageWithoutLicense == 1
		//UE_LOG(LogTemp, Warning, TEXT("Failed to read license file: %s"), *FilePath);
#else
		UE_LOG(LogTemp, Error, TEXT("Failed to read license file: %s"), *FilePath);
#endif
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