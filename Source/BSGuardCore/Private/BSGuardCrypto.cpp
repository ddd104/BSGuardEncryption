#include "BSGuardCrypto.h"
#include "BSCommonDefinition.h"


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
        bool ret = FMemory::Memcmp(Header, BSGE::CryptoMagic, 4) != 0;
        return ret;
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
    // 执行加密
    TArray<uint8> EncryptedData;
    TArray<uint8> IV;
    TArray<uint8> AuthTag;
    if (!Encrypt(PlainData, EncryptedData, IV, AuthTag))
    {
        UE_LOG(LogTemp, Error, TEXT("Encryption failed for file: %s"), *FilePath);
        return false;
    }
    // 组成最终文件数据: [BSGE::CryptoMagic][IV][CipherData][AuthTag]
    TArray<uint8> FileOut;
    FileOut.Append(BSGE::CryptoMagic, 4);
    FileOut.Append(IV);              // 12字节IV
    FileOut.Append(EncryptedData);   // 密文数据
    FileOut.Append(AuthTag);         // 16字节认证Tag
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
    // 检查长度是否至少包含头和Tag
    if (FileData.Num() < 4 + 12 + 16)
    {
        UE_LOG(LogTemp, Error, TEXT("File too small or not an encrypted asset: %s"), *FilePath);
        return false;
    }
    // 提取并验证魔数
    if (FMemory::Memcmp(FileData.GetData(), BSGE::CryptoMagic, 4) != 0)
    {
        UE_LOG(LogTemp, Error, TEXT("File %s is not recognized as encrypted (magic mismatch)."), *FilePath);
        return false;
    }
    // 提取IV和AuthTag
    TArray<uint8> IV;
    IV.Append(FileData.GetData() + 4, 12);
    TArray<uint8> AuthTag;
    AuthTag.Append(FileData.GetData() + FileData.Num() - 16, 16);
    // 提取密文主体
    int32 CipherOffset = 4 + 12;
    int32 CipherSize = FileData.Num() - CipherOffset - 16;
    if (CipherSize < 0) CipherSize = 0;
    TArray<uint8> CipherData;
    CipherData.Append(FileData.GetData() + CipherOffset, CipherSize);
    // 解密
    TArray<uint8> PlainData;
    if (!Decrypt(CipherData, IV, AuthTag, PlainData))
    {
        UE_LOG(LogTemp, Error, TEXT("Decryption failed or authentication tag mismatch for file: %s"), *FilePath);
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

bool FBSGuardCrypto::GenRandomBytes(uint8* Out, int32 Num)
{
    return RAND_bytes(Out, Num) == 1;
}
