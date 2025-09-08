//=============================================================
// Filename:       BSGuardEditor.h
// Publisher:      BigStar
// Creation Date:  2025-09-08
// Last Modified:  2025-09-08
// Version:        v1.0
//
// Description:
// Implement the function of crypto
//=============================================================

#include "BSGuardCrypto.h"
#include "BSCommonDefinition.h"
#include "BSGuardPlatformFile.h"
#include "BSLicenseUtils.h"


uint8 FBSGuardCrypto::Key[32];
bool FBSGuardCrypto::bKeyIsSet = false;
FRetBoolDelegate FBSGuardCrypto::bRetBoolDelegate;

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
    	//UE_LOG(LogTemp, Display, TEXT("%s, %d, File does not exist： %s"), ANSI_TO_TCHAR(__FUNCTION__), __LINE__, *AbsolutePath);
        return false;
    }
    
    // Read the file header 4 bytes to check the magic number
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
    	//UE_LOG(LogTemp, Display, TEXT("%s, %d, %s"), ANSI_TO_TCHAR(__FUNCTION__), __LINE__, *AbsolutePath);
        return false;
    }
    if (FileHandle->Read(Header, sizeof(Header)))
    {
    	//UE_LOG(LogTemp, Display, TEXT("%s, %d, %s"), ANSI_TO_TCHAR(__FUNCTION__), __LINE__, *AbsolutePath);
        return FMemory::Memcmp(Header, BSGE::CryptoMagic, 4) == 0;
    }
	//UE_LOG(LogTemp, Display, TEXT("%s, %d, %s"), ANSI_TO_TCHAR(__FUNCTION__), __LINE__, *AbsolutePath);
    return false;
}

bool FBSGuardCrypto::EncryptFile(const FString& FilePath)
{
    // Read the complete data of the original file
    TArray<uint8> PlainData;
    if (!FFileHelper::LoadFileToArray(PlainData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("[%d]Failed to read file for encryption: %s"), __LINE__, *FilePath);
        return false;
    }
    // Perform encryption
    TArray<uint8> EncryptedData;
    if (!Encrypt(PlainData, EncryptedData))
    {
        UE_LOG(LogTemp, Error, TEXT("[%d]Encryption failed for file: %s"), __LINE__, *FilePath);
        return false;
    }
    
    // Write files (overwrite the original file content) and directly use the underlying platform files to avoid re-encryption
    IPlatformFile* RawFile = FBSGuardPlatformFile::GetBottomPlatformFile();

	bool ret = RawFile->SetReadOnly(*FilePath, false);
    TUniquePtr<IFileHandle> FileHandle(RawFile->OpenWrite(*FilePath, false, true));
	if (!FileHandle )
	{
		UE_LOG(LogTemp, Error, TEXT("[%d]OpenWrite failed on bottom PF for: %s"), __LINE__, *FilePath);
		return false;
	}
    if (!FileHandle->Write(EncryptedData.GetData(), EncryptedData.Num()))
    {
        UE_LOG(LogTemp, Error, TEXT("[%d]Failed to write encrypted file: %s"), __LINE__, *FilePath);
        return false;
    }
    FileHandle->Flush(true);
    return true;
}

bool FBSGuardCrypto::DecryptFile(const FString& FilePath)
{
    if (!bKeyIsSet)
    {
        UE_LOG(LogTemp, Error, TEXT("[%d]Cannot decrypt file %s: Key not set."), __LINE__, *FilePath);
        return false;
    }
    // Read encrypted file data
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("[%d]Failed to read encrypted file: %s"), __LINE__, *FilePath);
        return false;
    }
	
    // Decryption is successful, write back the plaintext file data, and directly use the underlying platform file to avoid being encrypted
	IPlatformFile* Raw = FBSGuardPlatformFile::GetBottomPlatformFile();
	bool ret = Raw->SetReadOnly(*FilePath, false);
	TUniquePtr<IFileHandle> FileHandle(Raw->OpenWrite(*FilePath, false, true));
    if (!FileHandle )
    {
    	UE_LOG(LogTemp, Error, TEXT("[%d]OpenWrite failed on bottom PF for: %s"), __LINE__, *FilePath);
    	return false;
    }
    if (!FileHandle->Write(FileData.GetData(), FileData.Num()))
    {
        UE_LOG(LogTemp, Error, TEXT("[%d]Failed to write decrypted file: %s"), __LINE__, *FilePath);
        return false;
    }
    FileHandle->Flush(true);
    return true;
}

// Implementing AES-256-GCM encryption using the OpenSSL EVP interface
bool FBSGuardCrypto::Encrypt(const TArray<uint8>& InPlain, TArray<uint8>& OutCipher)
{
    const TArray<uint8>& SharedKey = FBSLicenseUtils::GetSharedKey();
    if (SharedKey.Num() != 32)
    {
        UE_LOG(LogTemp, Error, TEXT("SharedKey invalid"));
        return false;
    }

    // Generate Nonce
    uint8 Nonce[BSGE::GcmNonceSize];
    if (!GenRandomBytes(Nonce, BSGE::GcmNonceSize))
    {
        return false;
    }

    // Creating a Context
    EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
    if (!Ctx)
    {
        return false;
    }

    int32 PlainLen   = InPlain.Num();
    int32 CipherLen  = 0;
    int32 TmpLen     = 0;
    TArray<uint8> CipherText;
    CipherText.SetNumUninitialized(PlainLen);   // GCM No data expansion

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

    // Package：Magic|CEState|TEState|Ver|Nonce|Tag|Cipher
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

// Implementing AES-256-GCM decryption using the OpenSSL EVP interface
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
    if (SharedKey.Num() != 32)
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

bool FBSGuardCrypto::IsValidAction()
{
	bool ret = false;
	if (bRetBoolDelegate.IsBound())
	{
		ret = bRetBoolDelegate.Execute();
	}
	return ret;
}

bool FBSGuardCrypto::ShouldEncryptAsset(const FString& FilePath)
{
	FString AbsPath = FPaths::ConvertRelativePathToFull(FilePath);
	FString EngineDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
	FString SavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	if (AbsPath.StartsWith(EngineDir) || AbsPath.StartsWith(SavedDir))
	{
		// Skip engine content to avoid corrupting built-in assets
		//UE_LOG(LogTemp, Display, TEXT("%s, %d, %s"), ANSI_TO_TCHAR(__FUNCTION__), __LINE__, *FilePath);
		return false;
	}

	bool bIsAssetFile = FilePath.EndsWith(TEXT(".uasset")) || FilePath.EndsWith(TEXT(".utasset"));
	if (!bIsAssetFile)
	{
		//UE_LOG(LogTemp, Display, TEXT("%s, %d, %s"), ANSI_TO_TCHAR(__FUNCTION__), __LINE__, *FilePath);
		return false;
	}
	UE_LOG(LogTemp, Display, TEXT("%s, %d, %s"), ANSI_TO_TCHAR(__FUNCTION__), __LINE__, *FilePath);
	return true;
}

const FString& FBSGuardCrypto::ChooseHeaderExt(const FAssetData& AD)
{
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 27
	const bool bIsWorld = (AD.AssetClass == UWorld::StaticClass()->GetFName());
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0
	const bool bIsWorld = (AD.AssetClassPath == UWorld::StaticClass()->GetClassPathName());
#endif
	const bool bTextFormat = false;

	if (bIsWorld)
	{
		return bTextFormat
			? FPackageName::GetTextMapPackageExtension()   // ".utmap" :contentReference[oaicite:0]{index=0}
			: FPackageName::GetMapPackageExtension();      // ".umap"  :contentReference[oaicite:1]{index=1}
	}
	else
	{
		return bTextFormat
			? FPackageName::GetTextAssetPackageExtension() // ".utasset" :contentReference[oaicite:2]{index=2}
			: FPackageName::GetAssetPackageExtension();    // ".uasset"  :contentReference[oaicite:3]{index=3}
	}
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
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 27
			check(UBSGEncryptUserData::StaticClass() != nullptr);
			AUDOwner->AddAssetUserData(NewObject<UAssetUserData>(AUDOwner->_getUObject(), UBSGEncryptUserData::StaticClass()));
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0  
			AUDOwner->AddAssetUserDataOfClass(UBSGEncryptUserData::StaticClass());
#endif
			
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
