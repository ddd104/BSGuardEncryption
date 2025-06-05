#include "BSGuardFileHandleWrite.h"
#include "BSGuardCrypto.h"

FBSGuardPlatformFile::FBSGuardFileHandleWrite::FBSGuardFileHandleWrite(IFileHandle* InHandle): InnerHandle(InHandle)
{
	bError = false;
	// 生成随机IV并写入文件头 (Magic + IV 占 4+12 字节)
	InnerHandle->Write(BSGE::CryptoMagic, 4);
	IV.SetNumUninitialized(12);
	FBSGuardCrypto::GenRandomBytes(IV.GetData(), IV.Num());
	InnerHandle->Write(IV.GetData(), IV.Num());
	// 初始化OpenSSL加密CTX
	Ctx = EVP_CIPHER_CTX_new();
	const EVP_CIPHER* Cipher = EVP_aes_256_gcm();
	if (!Ctx || EVP_EncryptInit_ex(Ctx, Cipher, NULL, FBSGuardCrypto::Key, IV.GetData()) != 1)
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
