#pragma once
#include "CoreMinimal.h"

struct FBSLicenseData
{
	FString AssetPack;
	FString User;
	FString ExpireDate;
	FString SharedKey;
};

class BSGUARDCORE_API FBSLicenseUtils
{
public:
	// Load license from file and verify signature. Returns true on success.
	static bool LoadLicense(const FString& FilePath, FBSLicenseData& OutData);

	static FString GetPublicKeyPem();

	static TArray<uint8> GetSharedKey();

	static TArray<uint8> SharedKey;
};
