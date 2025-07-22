#include "BSGuardCrypto.h"
#include "BSCommonDefinition.h"
#include "BSLicenseUtils..h"


uint8 FBSGuardCrypto::Key[32];
bool FBSGuardCrypto::bKeyIsSet = false;

void FBSGuardCrypto::SetKey(const TArray<uint8>& InKey)
{
    if (InKey.Num() == 32)
    {
        FMemory::Memcpy(Key, InKey.GetData(), 32);
        bKeyIsSet = true;
    }
    else
    {
        bKeyIsSet = false;
    }
}

bool FBSGuardCrypto::HasValidKey()
{
    return bKeyIsSet;
}

bool FBSGuardCrypto::IsEncryptedAssetFile(const FString& FilePath)
{
    FString AbsolutePath = FilePath;
    if (FPaths::IsRelative(FilePath))
    {
        AbsolutePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), FilePath);
    }

    if (!FPaths::FileExists(AbsolutePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[IsEncryptedAssetFile] 文件不存在：%s"), *AbsolutePath);
        return false;
    }
    
    // 读取文件头4字节检查魔数
    uint8 Header[4] = {0};
    IPlatformFile& PlatFile = FPlatformFileManager::Get().GetPlatformFile();
    IPlatformFile* RawFile = PlatFile.GetLowerLevel();
    if (!RawFile)
    {
        RawFile = &PlatFile;
    }
    TUniquePtr<IFileHandle> FileHandle(RawFile->OpenRead(*AbsolutePath));
    if (!FileHandle)
    {
        return false;
    }
    if (FileHandle->Read(Header, sizeof(Header)))
    {
        return FMemory::Memcmp(Header, BSGE::CryptoMagic, 4) == 0;
    }
    return false;
}

bool FBSGuardCrypto::EncryptFile(const FString& FilePath)
{
    // 读取原文件完整数据
    TArray<uint8> PlainData;
    if (!FFileHelper::LoadFileToArray(PlainData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to read file for encryption: %s"), *FilePath);
        return false;
    }
    // 执行加密
    TArray<uint8> EncryptedData;
    if (!Encrypt(PlainData, EncryptedData))
    {
        UE_LOG(LogTemp, Error, TEXT("Encryption failed for file: %s"), *FilePath);
        return false;
    }
    
    // 写入文件（覆盖原文件内容），直接使用底层平台文件以避免再次被加密
    IPlatformFile& PlatFile = FPlatformFileManager::Get().GetPlatformFile();
    IPlatformFile* RawFile = PlatFile.GetLowerLevel();
    if (!RawFile)
    {
        RawFile = &PlatFile;
    }

    TUniquePtr<IFileHandle> FileHandle(RawFile->OpenWrite(*FilePath, false));
    if (!FileHandle || !FileHandle->Write(EncryptedData.GetData(), EncryptedData.Num()))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to write encrypted file: %s"), *FilePath);
        return false;
    }
    FileHandle->Flush(true);
    return true;
}

bool FBSGuardCrypto::DecryptFile(const FString& FilePath)
{
    if (!bKeyIsSet)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot decrypt file %s: Key not set."), *FilePath);
        return false;
    }
    // 读取加密文件数据
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to read encrypted file: %s"), *FilePath);
        return false;
    }
	
    // 解密成功，写回明文文件数据，直接使用底层平台文件以避免被加密
    IPlatformFile& PlatFile = FPlatformFileManager::Get().GetPlatformFile();
    IPlatformFile* RawFile = PlatFile.GetLowerLevel();
    if (!RawFile)
    {
        RawFile = &PlatFile;
    }
    TUniquePtr<IFileHandle> FileHandle(RawFile->OpenWrite(*FilePath, false));
    if (!FileHandle || !FileHandle->Write(FileData.GetData(), FileData.Num()))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to write decrypted file: %s"), *FilePath);
        return false;
    }
    FileHandle->Flush(true);
    return true;
}

// 使用OpenSSL EVP接口实现AES-256-GCM加密
bool FBSGuardCrypto::Encrypt(const TArray<uint8>& InPlain, TArray<uint8>& OutCipher)
{
    const TArray<uint8>& SharedKey = FBSLicenseUtils::GetSharedKey();
    if (SharedKey.Num() != 64)
    {
        UE_LOG(LogTemp, Error, TEXT("SharedKey invalid"));
        return false;
    }

    /* 1. 生成 12-byte Nonce */
    uint8 Nonce[BSGE::GcmNonceSize];
    if (!GenRandomBytes(Nonce, BSGE::GcmNonceSize))
    {
        return false;
    }

    /* 2. 创建上下文 */
    EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
    if (!Ctx)
    {
        return false;
    }

    int32 PlainLen   = InPlain.Num();
    int32 CipherLen  = 0;
    int32 TmpLen     = 0;
    TArray<uint8> CipherText;
    CipherText.SetNumUninitialized(PlainLen);   // GCM 不膨胀数据

    bool bOK =
        EVP_EncryptInit_ex(Ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl (Ctx, EVP_CTRL_GCM_SET_IVLEN, BSGE::GcmNonceSize, nullptr) == 1 &&
        EVP_EncryptInit_ex(Ctx, nullptr, nullptr, SharedKey.GetData(), Nonce)          == 1 &&
        EVP_EncryptUpdate (Ctx, CipherText.GetData(), &CipherLen,
                           InPlain.GetData(), PlainLen)                       == 1 &&
        EVP_EncryptFinal_ex(Ctx, CipherText.GetData() + CipherLen, &TmpLen)   == 1;

    CipherLen += TmpLen;

    uint8 Tag[BSGE::GcmTagSize];
    if (bOK)
    {
        bOK = EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_GET_TAG, BSGE::GcmTagSize, Tag) == 1;
    }
    EVP_CIPHER_CTX_free(Ctx);

    if (!bOK)
    {
        UE_LOG(LogTemp, Error, TEXT("Encrypt failed")); return false;
    }

    /* 组包：Magic|CEState|TEState|Ver|Nonce|Tag|Cipher */
    OutCipher.Reset();
    OutCipher.Append(BSGE::CryptoMagic, 4);
    OutCipher.Add(1);
    OutCipher.Add(1);
    OutCipher.Add(BSGE::CryptoVersion);
    OutCipher.Append(Nonce, BSGE::GcmNonceSize);
    OutCipher.Append(Tag, BSGE::GcmTagSize);
    OutCipher.Append(CipherText.GetData(), CipherLen);
    return true;
}

// 使用OpenSSL EVP接口实现AES-256-GCM解密
bool FBSGuardCrypto::Decrypt(const TArray<uint8>& InCipher, TArray<uint8>& OutPlain)
{
    const int32 MinSize = 2 + 4 + 1 + BSGE::GcmNonceSize + BSGE::GcmTagSize;
    if (InCipher.Num() < MinSize)
    {
        return false;
    }
    if (FMemory::Memcmp(InCipher.GetData(), BSGE::CryptoMagic, 4) != 0)
    {
        return false;
    }
    if (InCipher[6] != BSGE::CryptoVersion)
    {
        UE_LOG(LogTemp, Error, TEXT("Version mismatch")); return false;
    }
    const uint8* CEState = InCipher.GetData() + 5;
    const uint8* CTState = CEState + 1;
    const uint8* IV    = CTState + 1;
    const uint8* Nonce   = IV + BSGE::GcmNonceSize;
    const uint8* Tag  = Nonce + BSGE::GcmTagSize;
    const int32  DataLen = InCipher.Num() - MinSize;

    const TArray<uint8>& SharedKey = FBSLicenseUtils::GetSharedKey();
    if (SharedKey.Num() != 64)
    {
        return false;
    }

    EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
    if (!Ctx)
    {
        return false;
    }

    OutPlain.SetNumUninitialized(DataLen);

    int32 PlainLen = 0;
    int32 TmpLen   = 0;
    bool b1 = EVP_DecryptInit_ex(Ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1;
    bool b2 = EVP_CIPHER_CTX_ctrl (Ctx, EVP_CTRL_GCM_SET_IVLEN, BSGE::GcmNonceSize, nullptr) == 1;
    bool b3 = EVP_DecryptInit_ex(Ctx, nullptr, nullptr, SharedKey.GetData(), IV)          == 1;
    bool b4 = EVP_DecryptUpdate (Ctx, OutPlain.GetData(), &PlainLen, Tag, DataLen) == 1;
    bool b5 = EVP_CIPHER_CTX_ctrl (Ctx, EVP_CTRL_GCM_SET_TAG, BSGE::GcmTagSize, (void*)Nonce)  == 1;
    bool b6 = EVP_DecryptFinal_ex (Ctx, OutPlain.GetData() + PlainLen, &TmpLen)     == 1;
    bool bOK =
        EVP_DecryptInit_ex(Ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl (Ctx, EVP_CTRL_GCM_SET_IVLEN, BSGE::GcmNonceSize, nullptr) == 1 &&
        EVP_DecryptInit_ex(Ctx, nullptr, nullptr, SharedKey.GetData(), IV)          == 1 &&
        EVP_DecryptUpdate (Ctx, OutPlain.GetData(), &PlainLen, Tag, DataLen) == 1 &&
        EVP_CIPHER_CTX_ctrl (Ctx, EVP_CTRL_GCM_SET_TAG, BSGE::GcmTagSize, (void*)Nonce)  == 1 &&
        EVP_DecryptFinal_ex (Ctx, OutPlain.GetData() + PlainLen, &TmpLen)     == 1;

    EVP_CIPHER_CTX_free(Ctx);
    if (!bOK)
    {
        UE_LOG(LogTemp, Warning, TEXT("Decrypt auth failed"));
        return false;
    }

    OutPlain.SetNum(PlainLen + TmpLen);
    return true;
}

bool FBSGuardCrypto::GenRandomBytes(uint8* Out, int32 Num)
{
    return RAND_bytes(Out, Num) == 1;
}

bool FBSGuardCrypto::ShouldEncryptAsset(const FString& FilePath)
{
	FString AbsPath = FPaths::ConvertRelativePathToFull(FilePath);
	FString EngineDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
	FString SavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	if (AbsPath.StartsWith(EngineDir) || AbsPath.Contains(TEXT("/Engine/")) || AbsPath.StartsWith(SavedDir))
	{
		// Skip engine content to avoid corrupting built-in assets
		return false;
	}

	bool bIsAssetFile = FilePath.EndsWith(TEXT(".uasset")) ||
						FilePath.EndsWith(TEXT(".umap")) ||
						FilePath.EndsWith(TEXT(".utasset")) || FilePath.EndsWith(TEXT(".utmap"));
	if (!bIsAssetFile)
	{
		return false;
	}

	

	return true;
}


struct FBSGEncryptVersion
{
	enum class EBSGEncryptVer : int32
	{
		Plain      = 0,
		Encrypted  = 1,
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
	
	static const FGuid GUID;
};

const FGuid FBSGEncryptVersion::GUID(0x6E4DE065, 0x1F87468C, 0xA5F1E4D3, 0xDC97B2E9);
static FCustomVersionRegistration GBSGEncryptVer(
	FBSGEncryptVersion::GUID,
	(int32)FBSGEncryptVersion::EBSGEncryptVer::LatestVersion,
	TEXT("BSGE_EncryptTag"));

namespace BSGEncrypt
{
	void MarkPackageEncrypted(UPackage* Pkg)
	{
		if (!Pkg) return;


		UObject* MainAsset = Pkg->FindAssetInPackage();

		if (!MainAsset) return;

		if (IInterface_AssetUserData* AUDOwner = Cast<IInterface_AssetUserData>(MainAsset))
		{
			AUDOwner->AddAssetUserDataOfClass(UBSGEncryptUserData::StaticClass());
			Pkg->MarkPackageDirty();
			MainAsset->Modify();
		}
	}

	void MarkPackagePlain(UPackage* Pkg)
	{
		if (!Pkg) return;
		
		UObject* MainAsset = Pkg->FindAssetInPackage();

		if (!MainAsset) return;
		
		if (IInterface_AssetUserData* AUDOwner = Cast<IInterface_AssetUserData>(MainAsset))
		{
			//AUDOwner->AddAssetUserDataOfClass(UBSGPlainTag::StaticClass());
			AUDOwner->RemoveUserDataOfClass(UBSGEncryptUserData::StaticClass());
			Pkg->MarkPackageDirty();
			MainAsset->Modify();
		}
	}

	bool IsPackageEncrypted(UPackage* Pkg)
	{
		if (!Pkg) return false;
		
		UObject* MainAsset = Pkg->FindAssetInPackage();
		if (IInterface_AssetUserData* AUDOwner = Cast<IInterface_AssetUserData>(MainAsset))
		{
			bool bEncrypted = AUDOwner->GetAssetUserDataOfClass(UBSGEncryptUserData::StaticClass()) != nullptr;
			return bEncrypted;
		}
		return false;
	}

	bool IsObjectEncrypted(UObject* Obj)
	{
		if (IInterface_AssetUserData* AUDOwner = Cast<IInterface_AssetUserData>(Obj))
		{
			bool bEncrypted = AUDOwner->GetAssetUserDataOfClass(UBSGEncryptUserData::StaticClass()) != nullptr;
			return bEncrypted;
		}
		return false;
	}
}


void UBSGEncryptUserData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FBSGEncryptVersion::GUID);
	Ar.SetCustomVersion(FBSGEncryptVersion::GUID, (int32)FBSGEncryptVersion::EBSGEncryptVer::Encrypted, TEXT("BSGE_EncryptTag"));
}
