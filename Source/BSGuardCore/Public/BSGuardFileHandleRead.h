#pragma once
#include "BSGuardPlatformFile.h"

class FBSGuardPlatformFile::FBSGuardFileHandleRead : public IFileHandle
{
public:
    FBSGuardFileHandleRead(IFileHandle* InHandle);

    virtual ~FBSGuardFileHandleRead() {}

    // 读取接口：从解密后的数据缓冲读取
    virtual int64 Size() override;
    
    virtual int64 Tell() override;
    
    virtual bool Seek(int64 NewPosition) override;
    
    virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override;
    
    virtual bool Read(uint8* Destination, int64 BytesToRead) override;
    
    virtual bool Write(const uint8* Source, int64 BytesToWrite) override;
    
    virtual bool Flush(const bool bFullFlush = false) override { return true; }
    virtual bool Truncate(int64 NewSize) override { return false; }

    virtual bool ReadAt(uint8* Destination, int64 BytesToRead, int64 Offset) override;
private:
    IFileHandle* InnerHandle;
    TArray<uint8> EncryptedData;
    TArray<uint8> DecryptedData;
    int64 TotalSize;
    int64 ReadPos;

    uint8 CEState;
    uint8 CTState;
};
