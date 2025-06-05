#include "BSGuardFileHandleRead.h"

#include "BSCommonDefinition.h"
#include "BSGuardCrypto.h"

FBSGuardPlatformFile::FBSGuardFileHandleRead::FBSGuardFileHandleRead(IFileHandle* InHandle):InnerHandle(InHandle)
{
	// 获取文件大小以便读取全部内容
	TotalSize = InnerHandle->Size();
	// 读取整个文件密文到内存并解密（因为AES-GCM需要整体验证Tag）
	EncryptedData.SetNumUninitialized(TotalSize);
	InnerHandle->Seek(0);
	InnerHandle->Read(EncryptedData.GetData(), TotalSize);
	// 解析头、IV、Tag并进行解密
	if (EncryptedData.Num() >= 4 && FMemory::Memcmp(EncryptedData.GetData(), BSGE::CryptoMagic, 4) == 0)
	{
		// 提取IV、Tag、密文
		const int32 HeaderSize = 4 + 12;
		const int32 TagSize = 16;
		if (EncryptedData.Num() >= HeaderSize + TagSize)
		{
			TArray<uint8> IV;
			IV.Append(EncryptedData.GetData() + 4, 12);
			TArray<uint8> AuthTag;
			AuthTag.Append(EncryptedData.GetData() + EncryptedData.Num() - TagSize, TagSize);
			int32 CipherSize = EncryptedData.Num() - HeaderSize - TagSize;
			if (CipherSize < 0) CipherSize = 0;
			TArray<uint8> CipherData;
			CipherData.Append(EncryptedData.GetData() + HeaderSize, CipherSize);
			TArray<uint8> PlainData;
			if (FBSGuardCrypto::HasValidKey() && FBSGuardCrypto::Decrypt(CipherData, IV, AuthTag, PlainData))
			{
				DecryptedData = MoveTemp(PlainData);
			}
		}
	}
	// 清理底层资源（无需再用）
	delete InnerHandle;
	InnerHandle = nullptr;
	// 设置一个读指针
	ReadPos = 0;
}

int64 FBSGuardPlatformFile::FBSGuardFileHandleRead::Size()
{
	return DecryptedData.Num();
}

int64 FBSGuardPlatformFile::FBSGuardFileHandleRead::Tell()
{
	return ReadPos;
}

bool FBSGuardPlatformFile::FBSGuardFileHandleRead::Seek(int64 NewPosition)
{
	if (NewPosition >=0 && NewPosition <= DecryptedData.Num())
	{
		ReadPos = NewPosition;
		return true;
	}
	return false;
}

bool FBSGuardPlatformFile::FBSGuardFileHandleRead::SeekFromEnd(int64 NewPositionRelativeToEnd)
{
	return Seek(DecryptedData.Num() + NewPositionRelativeToEnd);
}

bool FBSGuardPlatformFile::FBSGuardFileHandleRead::Read(uint8* Destination, int64 BytesToRead)
{
	if (!Destination || BytesToRead < 0) return false;
	// 调整读取长度不超过可用数据
	int64 BytesAvailable = DecryptedData.Num() - ReadPos;
	int64 BytesToCopy = FMath::Min(BytesToRead, BytesAvailable);
	if (BytesToCopy <= 0) return false;
	FMemory::Memcpy(Destination, DecryptedData.GetData() + ReadPos, BytesToCopy);
	ReadPos += BytesToCopy;
	return BytesToCopy == BytesToRead;
}

bool FBSGuardPlatformFile::FBSGuardFileHandleRead::Write(const uint8* Source, int64 BytesToWrite)
{
	// 读取句柄不支持写
	return false;
}

bool FBSGuardPlatformFile::FBSGuardFileHandleRead::ReadAt(uint8* Destination, int64 BytesToRead, int64 Offset)
{
	return false;
}
