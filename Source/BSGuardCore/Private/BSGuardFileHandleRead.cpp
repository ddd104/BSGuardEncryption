#include "BSGuardFileHandleRead.h"

#include "BSCommonDefinition.h"
#include "BSGuardCrypto.h"
#include "Containers/StringConv.h"

FBSGuardPlatformFile::FBSGuardFileHandleRead::FBSGuardFileHandleRead(IFileHandle* InHandle):InnerHandle(InHandle)
{
	// 获取文件大小以便读取全部内容
	TotalSize = InnerHandle->Size();
	// 读取整个文件密文到内存并解密（因为AES-GCM需要整体验证Tag）
	EncryptedData.SetNumUninitialized(TotalSize);
	InnerHandle->Seek(0);
	InnerHandle->Read(EncryptedData.GetData(), TotalSize);
        // 解析头、IV、Tag并进行解密
        if (EncryptedData.Num() > 0 && FMemory::Memcmp(EncryptedData.GetData(), BSGE::CryptoMagic, 4) == 0)
        {
                int32 Offset = 4;
                uint8 Version = EncryptedData.IsValidIndex(Offset) ? EncryptedData[Offset++] : 0;
                if (Version == 0x02)
                {
                        uint8 EntryCount = EncryptedData[Offset++];
                        TArray<uint8> DataKey;
                        bool bGotKey = false;
                        for (uint8 i = 0; i < EntryCount; ++i)
                        {
                                uint8 IdLen = EncryptedData[Offset++];
                                FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(EncryptedData.GetData()+Offset), IdLen);
                                FString Id(Conv.Get(), Conv.Length());
                                Offset += IdLen;
                                int64 Expiry = 0;
                                FMemory::Memcpy(&Expiry, EncryptedData.GetData()+Offset, sizeof(int64));
                                Offset += sizeof(int64);
                                TArray<uint8> KeyIV;
                                KeyIV.Append(EncryptedData.GetData()+Offset, 12); Offset+=12;
                                TArray<uint8> EncDEK;
                                EncDEK.Append(EncryptedData.GetData()+Offset, 32); Offset+=32;
                                TArray<uint8> KeyTag;
                                KeyTag.Append(EncryptedData.GetData()+Offset, 16); Offset+=16;
                                if (!bGotKey && FBSGuardCrypto::HasValidKey() && Id == FBSGuardCrypto::GetCurrentUserId())
                                {
                                        if (FBSGuardCrypto::Decrypt(EncDEK, KeyIV, KeyTag, DataKey))
                                        {
                                                bGotKey = true;
                                        }
                                }
                        }
                        if (bGotKey)
                        {
                                TArray<uint8> DataIV;
                                DataIV.Append(EncryptedData.GetData()+Offset, 12); Offset += 12;
                                const int32 TagSize = 16;
                                int32 CipherSize = EncryptedData.Num() - Offset - TagSize;
                                if (CipherSize < 0) CipherSize = 0;
                                TArray<uint8> CipherData;
                                CipherData.Append(EncryptedData.GetData()+Offset, CipherSize);
                                TArray<uint8> DataTag;
                                DataTag.Append(EncryptedData.GetData()+Offset+CipherSize, TagSize);
                                if (FBSGuardCrypto::Decrypt(CipherData, DataIV, DataTag, DecryptedData))
                                {
                                        // success
                                }
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
