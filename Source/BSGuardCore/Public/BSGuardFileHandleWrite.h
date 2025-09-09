//=============================================================
// Filename:       BSGuardFileHandleWrite.h
// Publisher:      BigStar
// Creation Date:  2025-09-08
// Last Modified:  2025-09-08
// Version:        v1.0
//
// Description:
// Realize the writing functions of encrypted assets
//=============================================================

#pragma once
#include "BSGuardPlatformFile.h"
#include "BSCommonDefinition.h"

class FBSGuardPlatformFile::FBSGuardFileHandleWrite : public IFileHandle
{
public:
    FBSGuardFileHandleWrite(IFileHandle* InHandle);
    virtual ~FBSGuardFileHandleWrite();

    virtual int64 Tell() override;
    
    virtual int64 Size() override;
    
    virtual bool Seek(int64 NewPosition) override;
    
    virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override;
    
    virtual bool Read(uint8* Destination, int64 BytesToRead) override;

    virtual bool Write(const uint8* Source, int64 BytesToWrite) override;

    virtual bool Flush(const bool bFullFlush = false) override;

    virtual bool Truncate(int64 NewSize) override 
    {
        // 不支持截断
        return false;
    }
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 27
    
#else ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0
    virtual bool ReadAt(uint8* Destination, int64 BytesToRead, int64 Offset) override;
#endif
    
private:
    IFileHandle* InnerHandle;
    EVP_CIPHER_CTX* Ctx;
    TArray<uint8> IV;
    TArray<uint8> EncryptedBuffer;
    int64 TagOffset;
    bool bError;
};
