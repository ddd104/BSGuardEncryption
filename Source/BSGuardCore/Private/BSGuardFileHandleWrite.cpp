#include "BSGuardFileHandleWrite.h"
#include "BSGuardCrypto.h"
#include "Containers/StringConv.h"

FBSGuardPlatformFile::FBSGuardFileHandleWrite::FBSGuardFileHandleWrite(IFileHandle* InHandle): InnerHandle(InHandle)
{
        bError = false;
        // 文件头: Magic + Version + EntryCount + [Entry] + DataIV
        InnerHandle->Write(BSGE::CryptoMagic, 4);
        uint8 Version = 0x02;
        InnerHandle->Write(&Version, 1);
        uint8 EntryCount = 1;
        InnerHandle->Write(&EntryCount, 1);

        // 生成数据密钥和IV
        DataKey.SetNumUninitialized(32);
        FBSGuardCrypto::GenRandomBytes(DataKey.GetData(), DataKey.Num());
        DataIV.SetNumUninitialized(12);
        FBSGuardCrypto::GenRandomBytes(DataIV.GetData(), DataIV.Num());

        // 准备用户ID
        FTCHARToUTF8 UserIdUtf8(*FBSGuardCrypto::GetCurrentUserId());
        uint8 UserIdLen = FMath::Min<int32>(UserIdUtf8.Length(), 255);
        InnerHandle->Write(&UserIdLen, 1);
        InnerHandle->Write((const uint8*)UserIdUtf8.Get(), UserIdLen);

        int64 Expiry = 0; // 暂未存储过期时间
        InnerHandle->Write(reinterpret_cast<uint8*>(&Expiry), sizeof(int64));

        // 使用用户密钥加密数据密钥
        TArray<uint8> EncDEK;
        EncDEK.Reserve(32);
        KeyIV.SetNumUninitialized(12);
        TArray<uint8> KeyTag;
        if (!FBSGuardCrypto::Encrypt(DataKey, EncDEK, KeyIV, KeyTag))
        {
                bError = true;
        }
        InnerHandle->Write(KeyIV.GetData(), KeyIV.Num());
        InnerHandle->Write(EncDEK.GetData(), EncDEK.Num());
        InnerHandle->Write(KeyTag.GetData(), KeyTag.Num());

        // 写入数据IV供后续解密
        InnerHandle->Write(DataIV.GetData(), DataIV.Num());

        // 初始化OpenSSL加密CTX使用数据密钥
        Ctx = EVP_CIPHER_CTX_new();
        const EVP_CIPHER* Cipher = EVP_aes_256_gcm();
        if (!Ctx || EVP_EncryptInit_ex(Ctx, Cipher, NULL, DataKey.GetData(), DataIV.GetData()) != 1)
        {
                bError = true;
                UE_LOG(LogTemp, Error, TEXT("Failed to initialize AES-GCM context for writing."));
        }
}

FBSGuardPlatformFile::FBSGuardFileHandleWrite::~FBSGuardFileHandleWrite()
{
	// 析构时确保Flush输出Tag并释放资源
	Flush();
	if (Ctx)
	{
		EVP_CIPHER_CTX_free(Ctx);
		Ctx = nullptr;
	}
	delete InnerHandle;
	InnerHandle = nullptr;
}

int64 FBSGuardPlatformFile::FBSGuardFileHandleWrite::Tell()
{
	return InnerHandle ? InnerHandle->Tell() : -1;
}

int64 FBSGuardPlatformFile::FBSGuardFileHandleWrite::Size()
{
	return InnerHandle ? InnerHandle->Size() : -1;
}

bool FBSGuardPlatformFile::FBSGuardFileHandleWrite::Seek(int64 NewPosition)
{
	// 禁止随意Seek，按顺序写
	return false;
}

bool FBSGuardPlatformFile::FBSGuardFileHandleWrite::SeekFromEnd(int64 NewPositionRelativeToEnd)
{
	return false;
}

bool FBSGuardPlatformFile::FBSGuardFileHandleWrite::Read(uint8* Destination, int64 BytesToRead)
{
	return false;
}

bool FBSGuardPlatformFile::FBSGuardFileHandleWrite::Write(const uint8* Source, int64 BytesToWrite)
{
	if (bError || !InnerHandle) return false;
	// 加密源数据块
	int32 OutLen = 0;
	EncryptedBuffer.SetNumUninitialized(BytesToWrite);
	if (EVP_EncryptUpdate(Ctx, EncryptedBuffer.GetData(), &OutLen, Source, BytesToWrite) != 1)
	{
		UE_LOG(LogTemp, Error, TEXT("AES-GCM encryption update failed during write."));
		bError = true;
		return false;
	}
	// 将密文写入底层文件
	if (!InnerHandle->Write(EncryptedBuffer.GetData(), OutLen))
	{
		bError = true;
		return false;
	}
	return true;
}

bool FBSGuardPlatformFile::FBSGuardFileHandleWrite::Flush(const bool bFullFlush)
{
	if (bError || !InnerHandle) return false;
	// 完成加密，获取AuthTag并写入文件末尾
	int32 OutLen = 0;
	uint8 Dummy[16];
	if (EVP_EncryptFinal_ex(Ctx, Dummy, &OutLen) != 1)
	{
		UE_LOG(LogTemp, Error, TEXT("AES-GCM finalization failed."));
		bError = true;
		return false;
	}
	uint8 AuthTag[16];
	if (EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_GET_TAG, 16, AuthTag) != 1)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get AES-GCM auth tag."));
		bError = true;
		return false;
	}
	// 将认证Tag写入文件
	if (!InnerHandle->Write(AuthTag, 16))
	{
		bError = true;
		return false;
	}
	// 刷新文件缓冲
	return InnerHandle->Flush(bFullFlush);
}

bool FBSGuardPlatformFile::FBSGuardFileHandleWrite::ReadAt(uint8* Destination, int64 BytesToRead, int64 Offset)
{
	return false;
}
