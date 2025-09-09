//=============================================================
// Filename:       BSGuardPlatformFile.h
// Publisher:      BigStar
// Creation Date:  2025-09-08
// Last Modified:  2025-09-08
// Version:        v1.0
//
// Description:
// Implement encrypted asset file management
//=============================================================

#pragma once
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFilemanager.h"

/**
 * FGuardPlatformFile:
 * 自定义平台文件，用于拦截资产文件读写，实现透明加解密。
 * 作为对底层平台文件（Physical或Pak等）的包装。
 */
class FBSGuardPlatformFile : public IPlatformFile
{
public:
	FBSGuardPlatformFile();
	virtual ~FBSGuardPlatformFile();

	// 初始化包装
	bool Initialize(IPlatformFile* Inner, const TCHAR* Name);

	// IPlatformFile 接口实现:
	virtual IPlatformFile* GetLowerLevel() override;
	virtual const TCHAR* GetName() const override;

	virtual bool FileExists(const TCHAR* Filename) override;

	virtual int64 FileSize(const TCHAR* Filename) override;

	virtual bool DeleteFile(const TCHAR* Filename) override;

	virtual bool CreateDirectory(const TCHAR* DirPath) override;

	virtual bool DirectoryExists(const TCHAR* DirPath) override;

	virtual bool DeleteDirectory(const TCHAR* DirPath) override;

	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override;

	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) override;

	virtual bool IsReadOnly(const TCHAR* Filename) override;

	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override;

	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override;

	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override;

	virtual void SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) override;

	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) override;

	virtual FString GetFilenameOnDisk(const TCHAR* Filename) override;

	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override;

	virtual bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override;
	
	
	// 拦截核心读写接口:
	//virtual IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* Filename);
	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override;
	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override;
	
	static BSGUARDCORE_API IPlatformFile* GetBottomPlatformFile();

	friend class FBSGE_AssetActions;
private:
	bool IsEncryptedAssetFile2(FString FilePath);
	IPlatformFile* LowerLevel;  // 底层实际平台文件
	// 自定义文件句柄实现类声明（定义在cpp中）
	class FBSGuardFileHandleRead;
	class FBSGuardFileHandleWrite;

	BSGUARDCORE_API static TArray<FString> RecordAssetFilePath;
};
