//=============================================================
// Filename:       BSGuardPlatformFile.h
// Publisher:      BigStar
// Creation Date:  2025-09-08
// Last Modified:  2025-09-08
// Version:        v1.0
//
// Description:
// Implement the function of platform file operation
//=============================================================

#include "BSGuardPlatformFile.h"

#include "BSGuardCrypto.h"
#include "BSGuardFileHandleRead.h"
#include "BSGuardFileHandleWrite.h"


TArray<FString> FBSGuardPlatformFile::RecordAssetFilePath;

FBSGuardPlatformFile::FBSGuardPlatformFile(): LowerLevel(nullptr)
{}

FBSGuardPlatformFile::~FBSGuardPlatformFile()
{}

IPlatformFile* FBSGuardPlatformFile::GetLowerLevel()
{
	return LowerLevel;
}

const TCHAR* FBSGuardPlatformFile::GetName() const
{ return TEXT("BSGuardPlatformFile"); }

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
	UE_LOG(LogTemp, Display, TEXT("FBSGuardPlatformFile::Initialize"));
	if (Inner == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("FBSGuardPlatformFile::Initialize: Inner is nullptr"));
		return false;
	}
	UE_LOG(LogTemp, Display, TEXT("FBSGuardPlatformFile::Initialize "));
	LowerLevel = Inner;
	return true;
}

static bool IsRunningCookCommandlet2()
{
	FString Commandline = FCommandLine::Get();
	const bool bIsCookCommandlet = IsRunningCommandlet() && Commandline.Contains(TEXT("run=cook"));
	return bIsCookCommandlet;
}

IFileHandle* FBSGuardPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	//UE_LOG(LogTemp, Display, TEXT("%s, %d"), ANSI_TO_TCHAR(__FUNCTION__), __LINE__);
	FString FilePath(Filename);
	// Only block asset files with specific extensions
	if (FBSGuardCrypto::ShouldEncryptAsset(FilePath) && FBSGuardCrypto::IsEncryptedAssetFile(FilePath))
	{
		UE_LOG(LogTemp, Display, TEXT("%s, %d, %s"), ANSI_TO_TCHAR(__FUNCTION__), __LINE__, *FilePath);
		// Use a custom read handle to provide data in decrypted form
		IFileHandle* InnerHandle = LowerLevel->OpenRead(Filename, bAllowWrite);
		if (!InnerHandle)
		{
			return nullptr;
		}
		/*UE_LOG(LogTemp, Display, TEXT("%s, %d, %s"), ANSI_TO_TCHAR(__FUNCTION__), __LINE__, *FilePath);
		if (!FBSGuardCrypto::IsValidAction())
		{
			return InnerHandle;
		}*/
#if WITH_CanPackagingWithEncryption == 0
		if (IsRunningCommandlet() || IsRunningCookCommandlet2())
		{
			return InnerHandle;
		}
#endif
		
		UE_LOG(LogTemp, Display, TEXT("%d, %s"), __LINE__, *FilePath);
		return new FBSGuardFileHandleRead(InnerHandle);
	}
	//UE_LOG(LogTemp, Display, TEXT("%s, %d"), ANSI_TO_TCHAR(__FUNCTION__), __LINE__);
	return LowerLevel->OpenRead(Filename, bAllowWrite);
}

IFileHandle* FBSGuardPlatformFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
	//UE_LOG(LogTemp, Display, TEXT("%s, %d"), ANSI_TO_TCHAR(__FUNCTION__), __LINE__);
	FString FilePath(Filename);
	// When used for export
	if (IsEncryptedAssetFile2(FilePath))
	{
		IFileHandle* InnerHandle = LowerLevel->OpenWrite(Filename, false, bAllowRead);
		if (!InnerHandle)
		{
			return nullptr;
		}
		/*if (!FBSGuardCrypto::IsValidAction())
		{
			return InnerHandle;
		}*/
		UE_LOG(LogTemp, Display, TEXT("%d, %s"), __LINE__, *FilePath);
		return new FBSGuardFileHandleWrite(InnerHandle);
	}
	//UE_LOG(LogTemp, Display, TEXT("%s, %d"), ANSI_TO_TCHAR(__FUNCTION__), __LINE__);
	return LowerLevel->OpenWrite(Filename, bAppend, bAllowRead);
}

IPlatformFile* FBSGuardPlatformFile::GetBottomPlatformFile()
{
	// Ensure that the IPlatformFile obtained is the lowest level
	IPlatformFile* Top = &FPlatformFileManager::Get().GetPlatformFile();
	IPlatformFile* Bottom = Top;
	/*for (IPlatformFile* LL = Top->GetLowerLevel(); LL; LL = LL->GetLowerLevel())
	{
		Bottom = LL;
		UE_LOG(LogTemp,Warning,TEXT("PF: %s"), LL->GetName());
	}*/
	UE_LOG(LogTemp,Warning,TEXT("Bottom PF: %s"), Bottom->GetName());
	return Bottom;
}

static bool AreSameFilename(const FString& A, const FString& B, bool bIgnoreExtension = false)
{
	
	FString A1 = A, B1 = B;
	A1.TrimStartAndEndInline();
	B1.TrimStartAndEndInline();
	
	FPaths::NormalizeFilename(A1);
	FPaths::NormalizeFilename(B1);

	const FString NameA = bIgnoreExtension ? FPaths::GetBaseFilename(A1) : FPaths::GetCleanFilename(A1);
	const FString NameB = bIgnoreExtension ? FPaths::GetBaseFilename(B1) : FPaths::GetCleanFilename(B1);
	
	return NameA.Equals(NameB, ESearchCase::IgnoreCase);
}

bool FBSGuardPlatformFile::IsEncryptedAssetFile2(FString FilePath)
{
	for (auto& AssetData : RecordAssetFilePath)
	{
		if (AreSameFilename(FilePath, AssetData, true))
		{
			return true;
		}
	}
	return false;
}
