#include "BSGuardPlatformFile.h"

#include "BSGuardCrypto.h"
#include "BSGuardFileHandleRead.h"
#include "BSGuardFileHandleWrite.h"

FBSGuardPlatformFile::FBSGuardPlatformFile(): LowerLevel(nullptr)
{}

FBSGuardPlatformFile::~FBSGuardPlatformFile()
{}

IPlatformFile* FBSGuardPlatformFile::GetLowerLevel()
{ return LowerLevel; }

const TCHAR* FBSGuardPlatformFile::GetName() const
{ return TEXT("GuardPlatformFile"); }

bool FBSGuardPlatformFile::FileExists(const TCHAR* Filename)
{
	return LowerLevel->FileExists(Filename);
}

int64 FBSGuardPlatformFile::FileSize(const TCHAR* Filename)
{
	return LowerLevel->FileSize(Filename);
}

bool FBSGuardPlatformFile::DeleteFile(const TCHAR* Filename)
{
	return LowerLevel->DeleteFile(Filename);
}

bool FBSGuardPlatformFile::CreateDirectory(const TCHAR* DirPath)
{
	return LowerLevel->CreateDirectory(DirPath);
}

bool FBSGuardPlatformFile::DirectoryExists(const TCHAR* DirPath)
{
	return LowerLevel->DirectoryExists(DirPath);
}

bool FBSGuardPlatformFile::DeleteDirectory(const TCHAR* DirPath)
{
	return LowerLevel->DeleteDirectory(DirPath);
}

FFileStatData FBSGuardPlatformFile::GetStatData(const TCHAR* FilenameOrDirectory)
{
	return LowerLevel->GetStatData(FilenameOrDirectory);
}

void FBSGuardPlatformFile::SetLowerLevel(IPlatformFile* NewLowerLevel)
{
	//return LowerLevel->SetLowerLevel(NewLowerLevel);
	LowerLevel = NewLowerLevel;
}

bool FBSGuardPlatformFile::IsReadOnly(const TCHAR* Filename)
{
	return LowerLevel->IsReadOnly(Filename);
}

bool FBSGuardPlatformFile::MoveFile(const TCHAR* To, const TCHAR* From)
{
	return LowerLevel->MoveFile(To, From);
}

bool FBSGuardPlatformFile::SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue)
{
	return LowerLevel->SetReadOnly(Filename, bNewReadOnlyValue);
}

FDateTime FBSGuardPlatformFile::GetTimeStamp(const TCHAR* Filename)
{
	return LowerLevel->GetTimeStamp(Filename);
}

void FBSGuardPlatformFile::SetTimeStamp(const TCHAR* Filename, FDateTime DateTime)
{
	return LowerLevel->SetTimeStamp(Filename, DateTime);
}

FDateTime FBSGuardPlatformFile::GetAccessTimeStamp(const TCHAR* Filename)
{
	return LowerLevel->GetAccessTimeStamp(Filename);
}

FString FBSGuardPlatformFile::GetFilenameOnDisk(const TCHAR* Filename)
{
	return LowerLevel->GetFilenameOnDisk(Filename);
}

bool FBSGuardPlatformFile::IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor)
{
	return LowerLevel->IterateDirectory(Directory, Visitor);
}

bool FBSGuardPlatformFile::IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor)
{
	return LowerLevel->IterateDirectoryStat(Directory, Visitor);
}

bool FBSGuardPlatformFile::Initialize(IPlatformFile* Inner, const TCHAR* Name)
{
	if (Inner == nullptr) return false;
	LowerLevel = Inner;
	return true;
}

IFileHandle* FBSGuardPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	FString FilePath(Filename);
	// 只拦截特定扩展名的资产文件
	if (FBSGuardCrypto::ShouldEncryptAsset(FilePath) && FBSGuardCrypto::IsEncryptedAssetFile(FilePath))
	{
		// 使用自定义读取句柄，以解密方式提供数据
		IFileHandle* InnerHandle = LowerLevel->OpenRead(Filename, bAllowWrite);
		if (!InnerHandle)
		{
			return nullptr;
		}
		return new FBSGuardFileHandleRead(InnerHandle);
	}
	// 对于不拦截的文件，直接调用底层
	return LowerLevel->OpenRead(Filename, bAllowWrite);
}

IFileHandle* FBSGuardPlatformFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
	FString FilePath(Filename);
	if (FBSGuardCrypto::ShouldEncryptAsset(FilePath))
	{
		if (!FBSGuardCrypto::HasValidKey())
		{
			// 如果没有密钥，不进行加密，直接写明文（也可以选择阻止写入以避免产生未加密文件，这里允许明文写以不中断工作流）
			return LowerLevel->OpenWrite(Filename, bAppend, bAllowRead);
		}
		// 打开底层写句柄（写入临时缓存文件或内存）
		// 我们将创建自定义句柄以在写入过程中加密数据
		IFileHandle* InnerHandle = LowerLevel->OpenWrite(Filename, false, bAllowRead);
		if (!InnerHandle)
		{
			return nullptr;
		}
		return new FBSGuardFileHandleWrite(InnerHandle);
	}
	// 非资产文件正常写
	return LowerLevel->OpenWrite(Filename, bAppend, bAllowRead);
}
