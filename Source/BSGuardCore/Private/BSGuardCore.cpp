// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSGuardCore.h"

#include "BSGuardCrypto.h"
#include "BSGuardPlatformFile.h"
#include "BSGuardSettings.h"

#define LOCTEXT_NAMESPACE "FBSGuardCoreModule"



struct FBSGE_EarlyInstaller
{
	FBSGE_EarlyInstaller()
	{
		UE_LOG(LogTemp, Display, TEXT("FBSGE_EarlyInstaller"));
		// Save the original platform file pointer
		OriginalPlatformFile = &FPlatformFileManager::Get().GetPlatformFile();
		// Create our custom platform file and initialize
		GuardPlatformFile = MakeUnique<FBSGuardPlatformFile>();
		GuardPlatformFile->Initialize(OriginalPlatformFile, TEXT("BSGuardPlatformFile"));
		// Replace the current platform file with GuardPlatformFile, making it the top level
		FPlatformFileManager::Get().SetPlatformFile(*GuardPlatformFile);
		UE_LOG(LogTemp, Display, TEXT("FBSGE_EarlyInstaller 222"));

		BSGuardSettings = MakeShared<FBSGuardSettings>();
		if (BSGuardSettings)
		{
			if (BSGuardSettings->ValidateAndSetKey())
			{
				// Set the verified key bytes to the encryption module
				FBSGuardCrypto::SetKey(BSGuardSettings->GetKeyBytes());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("GuardEncryption: No valid key provided. Encryption will remain disabled."));
			}
		}
		UE_LOG(LogTemp, Log, TEXT("GuardEncryption: Custom platform file installed. Encryption %s."), 
			   FBSGuardCrypto::HasValidKey() ? TEXT("ENABLED") : TEXT("DISABLED"));
	}
	
public:
	IPlatformFile* OriginalPlatformFile;
	TUniquePtr<class FBSGuardPlatformFile> GuardPlatformFile;

	TSharedPtr<class FBSGuardSettings> BSGuardSettings;
};  

#if WITH_CanPackagingWithEncryption == 1
static FBSGE_EarlyInstaller BSGE_PlatformFileAutoInstaller;
#endif

IPlatformFile* FBSGuardCoreModule::GetPlatformFile()
{
	static FBSGuardPlatformFile Guard;
	return &Guard;
}

void FBSGuardCoreModule::StartupModule()
{
#if WITH_CanPackagingWithEncryption == 0
	static FBSGE_EarlyInstaller BSGE_PlatformFileAutoInstaller;
#endif
	UE_LOG(LogTemp, Log, TEXT("FBSGuardCoreModule::StartupModule"));
}

void FBSGuardCoreModule::ShutdownModule()
{
	
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBSGuardCoreModule, BSGuardCore)