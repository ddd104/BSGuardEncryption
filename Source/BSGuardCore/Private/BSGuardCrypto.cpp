#include "BSGuardCrypto.h"
#include "BSCommonDefinition.h"


uint8 FBSGuardCrypto::Key[32];
bool FBSGuardCrypto::bKeyIsSet = false;
FString FBSGuardCrypto::CurrentUserId;

void FBSGuardCrypto::SetKey(const TArray<uint8>& InKey, const FString& InUserId)
{
    if (InKey.Num() == 32)
    {
        FMemory::Memcpy(Key, InKey.GetData(), 32);
        bKeyIsSet = true;
        CurrentUserId = InUserId;
    }
    else
    {
        bKeyIsSet = false;
        CurrentUserId.Reset();
    }
}

bool FBSGuardCrypto::HasValidKey()
{
    return bKeyIsSet;
}

const FString& FBSGuardCrypto::GetCurrentUserId()
{
    return CurrentUserId;
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
    TUniquePtr<IFileHandle> FileHandle(PlatFile.OpenRead(*AbsolutePath));
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
    if (!bKeyIsSet)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot encrypt file %s: Key not set."), *FilePath);
        return false;
    }
    // 读取原文件完整数据
    TArray<uint8> PlainData;
    if (!FFileHelper::LoadFileToArray(PlainData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to read file for encryption: %s"), *FilePath);
        return false;
    }
    // 数据密钥加密资产内容
    TArray<uint8> DataKey;
    DataKey.SetNumUninitialized(32);
    GenRandomBytes(DataKey.GetData(), DataKey.Num());
    TArray<uint8> DataIV;
    TArray<uint8> DataTag;
    TArray<uint8> DataCipher;
    if (!EncryptWithKey(PlainData, DataKey.GetData(), DataCipher, DataIV, DataTag))
    {
        UE_LOG(LogTemp, Error, TEXT("Encryption failed for file: %s"), *FilePath);
        return false;
    }

    // 使用用户密钥封装数据密钥
    TArray<uint8> KeyIV;
    TArray<uint8> EncDEK;
    TArray<uint8> KeyTag;
    if (!EncryptWithKey(DataKey, Key, EncDEK, KeyIV, KeyTag))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to encrypt data key for file: %s"), *FilePath);
        return false;
    }

    // 写入文件头
    TArray<uint8> FileOut;
    FileOut.Append(BSGE::CryptoMagic, 4);
    uint8 Version = 0x02;
    FileOut.Add(Version);
    FileOut.Add(1); // entry count

    FTCHARToUTF8 UserUtf8(*CurrentUserId);
    uint8 IdLen = FMath::Min<int32>(UserUtf8.Length(), 255);
    FileOut.Add(IdLen);
    FileOut.Append((uint8*)UserUtf8.Get(), IdLen);
    int64 Expiry = 0;
    FileOut.Append((uint8*)&Expiry, sizeof(int64));
    FileOut.Append(KeyIV);
    FileOut.Append(EncDEK);
    FileOut.Append(KeyTag);
    FileOut.Append(DataIV);
    FileOut.Append(DataCipher);
    FileOut.Append(DataTag);
    // 写入文件（覆盖原文件内容）
    if (!FFileHelper::SaveArrayToFile(FileOut, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to write encrypted file: %s"), *FilePath);
        return false;
    }
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
    if (FileData.Num() < 4)
    {
        return false;
    }
    if (FMemory::Memcmp(FileData.GetData(), BSGE::CryptoMagic, 4) != 0)
    {
        UE_LOG(LogTemp, Error, TEXT("File %s is not recognized as encrypted."), *FilePath);
        return false;
    }
    int32 Offset = 4;
    uint8 Version = FileData[Offset++];
    if (Version != 0x02)
    {
        UE_LOG(LogTemp, Error, TEXT("Unsupported encryption version for file: %s"), *FilePath);
        return false;
    }
    uint8 EntryCount = FileData[Offset++];
    TArray<uint8> DataKey;
    bool bFound = false;
    for (uint8 i = 0; i < EntryCount; ++i)
    {
        uint8 IdLen = FileData[Offset++];
        FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(FileData.GetData()+Offset), IdLen);
        FString Id(Conv.Get(), Conv.Length());
        Offset += IdLen;
        Offset += sizeof(int64); // skip expiry
        TArray<uint8> KeyIV; KeyIV.Append(FileData.GetData()+Offset,12); Offset+=12;
        TArray<uint8> EncDEK; EncDEK.Append(FileData.GetData()+Offset,32); Offset+=32;
        TArray<uint8> KeyTag; KeyTag.Append(FileData.GetData()+Offset,16); Offset+=16;
        if (!bFound && Id == CurrentUserId)
        {
            if (DecryptWithKey(EncDEK, Key, KeyIV, KeyTag, DataKey))
            {
                bFound = true;
            }
        }
    }
    if (!bFound)
    {
        UE_LOG(LogTemp, Error, TEXT("No matching key entry for file: %s"), *FilePath);
        return false;
    }
    TArray<uint8> DataIV; DataIV.Append(FileData.GetData()+Offset,12); Offset+=12;
    const int32 TagSize = 16;
    int32 CipherSize = FileData.Num() - Offset - TagSize;
    if (CipherSize < 0) CipherSize = 0;
    TArray<uint8> CipherData; CipherData.Append(FileData.GetData()+Offset, CipherSize);
    TArray<uint8> DataTag; DataTag.Append(FileData.GetData()+Offset+CipherSize, TagSize);
    TArray<uint8> PlainData;
    if (!DecryptWithKey(CipherData, DataKey.GetData(), DataIV, DataTag, PlainData))
    {
        UE_LOG(LogTemp, Error, TEXT("Decryption failed for file: %s"), *FilePath);
        return false;
    }
    // 解密成功，写回明文文件数据
    if (!FFileHelper::SaveArrayToFile(PlainData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to write decrypted file: %s"), *FilePath);
        return false;
    }
    return true;
}

// 使用OpenSSL EVP接口实现AES-256-GCM加密
bool FBSGuardCrypto::Encrypt(const TArray<uint8>& PlainData, TArray<uint8>& OutEncryptedData,
                                     TArray<uint8>& OutIV, TArray<uint8>& OutAuthTag)
{
    // 生成随机12字节IV
    OutIV.SetNumUninitialized(12);
    GenRandomBytes(OutIV.GetData(), OutIV.Num());
    EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
    if (!Ctx) return false;
    const EVP_CIPHER* Cipher = EVP_aes_256_gcm();
    int32 OutLen = 0;
    int32 CipherLen = 0;
    // 初始化加密CTX
    if (EVP_EncryptInit_ex(Ctx, Cipher, NULL, Key, OutIV.GetData()) != 1)
    {
        EVP_CIPHER_CTX_free(Ctx);
        return false;
    }
    // 分配输出缓冲区
    OutEncryptedData.SetNumUninitialized(PlainData.Num());
    // 加密更新
    if (PlainData.Num() > 0)
    {
        if (EVP_EncryptUpdate(Ctx, OutEncryptedData.GetData(), &OutLen, PlainData.GetData(), PlainData.Num()) != 1)
        {
            EVP_CIPHER_CTX_free(Ctx);
            return false;
        }
        CipherLen = OutLen;
    }
    // 加密Final（GCM模式下不会输出数据，但需调用以计算AuthTag）
    if (EVP_EncryptFinal_ex(Ctx, OutEncryptedData.GetData() + CipherLen, &OutLen) != 1)
    {
        EVP_CIPHER_CTX_free(Ctx);
        return false;
    }
    CipherLen += OutLen;
    OutEncryptedData.SetNum(CipherLen); // 调整密文长度
    // 获取认证Tag（16字节）
    OutAuthTag.SetNumUninitialized(16);
    if (EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_GET_TAG, 16, OutAuthTag.GetData()) != 1)
    {
        EVP_CIPHER_CTX_free(Ctx);
        return false;
    }
    EVP_CIPHER_CTX_free(Ctx);
    return true;
}

// 使用OpenSSL EVP接口实现AES-256-GCM解密
bool FBSGuardCrypto::Decrypt(const TArray<uint8>& EncryptedData, const TArray<uint8>& IV,
                                     const TArray<uint8>& AuthTag, TArray<uint8>& OutPlainData)
{
    if (IV.Num() != 12 || AuthTag.Num() != 16)
    {
        return false;
    }
    EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
    if (!Ctx) return false;
    const EVP_CIPHER* Cipher = EVP_aes_256_gcm();
    int32 OutLen = 0;
    int32 PlainLen = 0;
    // 初始化解密
    if (EVP_DecryptInit_ex(Ctx, Cipher, NULL, Key, IV.GetData()) != 1)
    {
        EVP_CIPHER_CTX_free(Ctx);
        return false;
    }
    // 提供密文数据
    OutPlainData.SetNumUninitialized(EncryptedData.Num());
    if (EncryptedData.Num() > 0)
    {
        if (EVP_DecryptUpdate(Ctx, OutPlainData.GetData(), &OutLen, EncryptedData.GetData(), EncryptedData.Num()) != 1)
        {
            EVP_CIPHER_CTX_free(Ctx);
            return false;
        }
        PlainLen = OutLen;
    }
    // 设置期望的AuthTag用于验证
    if (EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_SET_TAG, AuthTag.Num(), (void*)AuthTag.GetData()) != 1)
    {
        EVP_CIPHER_CTX_free(Ctx);
        return false;
    }
    // Final解密并验证Tag
    if (EVP_DecryptFinal_ex(Ctx, OutPlainData.GetData() + PlainLen, &OutLen) != 1)
    {
        // 验证失败（可能密钥不对或数据被篡改）
        EVP_CIPHER_CTX_free(Ctx);
        return false;
    }
    PlainLen += OutLen;
    OutPlainData.SetNum(PlainLen);
    EVP_CIPHER_CTX_free(Ctx);
    return true;
}

bool FBSGuardCrypto::EncryptWithKey(const TArray<uint8>& PlainData, const uint8* InKey, TArray<uint8>& OutEncryptedData,
                                    TArray<uint8>& OutIV, TArray<uint8>& OutAuthTag)
{
    OutIV.SetNumUninitialized(12);
    GenRandomBytes(OutIV.GetData(), OutIV.Num());
    EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
    if (!Ctx) return false;
    const EVP_CIPHER* Cipher = EVP_aes_256_gcm();
    int32 OutLen = 0;
    int32 CipherLen = 0;
    if (EVP_EncryptInit_ex(Ctx, Cipher, NULL, InKey, OutIV.GetData()) != 1)
    {
        EVP_CIPHER_CTX_free(Ctx);
        return false;
    }
    OutEncryptedData.SetNumUninitialized(PlainData.Num());
    if (PlainData.Num() > 0)
    {
        if (EVP_EncryptUpdate(Ctx, OutEncryptedData.GetData(), &OutLen, PlainData.GetData(), PlainData.Num()) != 1)
        {
            EVP_CIPHER_CTX_free(Ctx);
            return false;
        }
        CipherLen = OutLen;
    }
    if (EVP_EncryptFinal_ex(Ctx, OutEncryptedData.GetData() + CipherLen, &OutLen) != 1)
    {
        EVP_CIPHER_CTX_free(Ctx);
        return false;
    }
    CipherLen += OutLen;
    OutEncryptedData.SetNum(CipherLen);
    OutAuthTag.SetNumUninitialized(16);
    if (EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_GET_TAG, 16, OutAuthTag.GetData()) != 1)
    {
        EVP_CIPHER_CTX_free(Ctx);
        return false;
    }
    EVP_CIPHER_CTX_free(Ctx);
    return true;
}

bool FBSGuardCrypto::DecryptWithKey(const TArray<uint8>& EncryptedData, const uint8* InKey,
                                    const TArray<uint8>& IV, const TArray<uint8>& AuthTag, TArray<uint8>& OutPlainData)
{
    if (IV.Num() != 12 || AuthTag.Num() != 16)
    {
        return false;
    }
    EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
    if (!Ctx) return false;
    const EVP_CIPHER* Cipher = EVP_aes_256_gcm();
    int32 OutLen = 0;
    int32 PlainLen = 0;
    if (EVP_DecryptInit_ex(Ctx, Cipher, NULL, InKey, IV.GetData()) != 1)
    {
        EVP_CIPHER_CTX_free(Ctx);
        return false;
    }
    OutPlainData.SetNumUninitialized(EncryptedData.Num());
    if (EncryptedData.Num() > 0)
    {
        if (EVP_DecryptUpdate(Ctx, OutPlainData.GetData(), &OutLen, EncryptedData.GetData(), EncryptedData.Num()) != 1)
        {
            EVP_CIPHER_CTX_free(Ctx);
            return false;
        }
        PlainLen = OutLen;
    }
    if (EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_SET_TAG, AuthTag.Num(), (void*)AuthTag.GetData()) != 1)
    {
        EVP_CIPHER_CTX_free(Ctx);
        return false;
    }
    if (EVP_DecryptFinal_ex(Ctx, OutPlainData.GetData() + PlainLen, &OutLen) != 1)
    {
        EVP_CIPHER_CTX_free(Ctx);
        return false;
    }
    PlainLen += OutLen;
    OutPlainData.SetNum(PlainLen);
    EVP_CIPHER_CTX_free(Ctx);
    return true;
}

bool FBSGuardCrypto::GenRandomBytes(uint8* Out, int32 Num)
{
    return RAND_bytes(Out, Num) == 1;
}
