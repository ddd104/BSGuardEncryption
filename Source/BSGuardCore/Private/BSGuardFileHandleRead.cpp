#include "BSGuardFileHandleRead.h"

#include "BSCommonDefinition.h"
#include "BSGuardCrypto.h"

FBSGuardPlatformFile::FBSGuardFileHandleRead::FBSGuardFileHandleRead(IFileHandle* InHandle):InnerHandle(InHandle)
{
	TotalSize = InnerHandle->Size();
	EncryptedData.SetNumUninitialized(TotalSize);
	InnerHandle->Seek(4);
	CEState = InnerHandle->Read(EncryptedData.GetData(), 1);
	InnerHandle->Seek(5);
	CTState = InnerHandle->Read(EncryptedData.GetData(), 1);
	InnerHandle->Seek(0);
	InnerHandle->Read(EncryptedData.GetData(), TotalSize);
	// 解析头、IV、Tag并进行解密
	if (EncryptedData.Num() >= 4 && FMemory::Memcmp(EncryptedData.GetData(), BSGE::CryptoMagic, 4) == 0)
	{
		TArray<uint8> PlainData;
		if (FBSGuardCrypto::Decrypt(EncryptedData, PlainData))
		{
			DecryptedData = MoveTemp(PlainData);
		}
	}
	delete InnerHandle;
	InnerHandle = nullptr;
	// set read pointer to start
	ReadPos = 0;
}

FBSGuardPlatformFile::FBSGuardFileHandleRead::~FBSGuardFileHandleRead()
{
	delete InnerHandle;
	InnerHandle = nullptr; 
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
	if (!Destination || BytesToRead < 0)
	{
		return false;
	}
	// 调整读取长度不超过可用数据
	int64 BytesAvailable = DecryptedData.Num() - ReadPos;
	int64 BytesToCopy = FMath::Min(BytesToRead, BytesAvailable);
	if (BytesToCopy <= 0)
	{
		return false;
	}
	FMemory::Memcpy(Destination, DecryptedData.GetData() + ReadPos, BytesToCopy);
	ReadPos += BytesToCopy;
	return BytesToCopy == BytesToRead;
}

bool FBSGuardPlatformFile::FBSGuardFileHandleRead::Write(const uint8* Source, int64 BytesToWrite)
{
	// 读取句柄不支持写
	return false;
}
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 27
    
#else ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0
bool FBSGuardPlatformFile::FBSGuardFileHandleRead::ReadAt(uint8* Destination, int64 BytesToRead, int64 Offset)
{
	return false;
}
#endif


