//=============================================================
// Filename:       BSGuardEditor.h
// Publisher:      BigStar
// Creation Date:  2025-09-08
// Last Modified:  2025-09-08
// Version:        v1.0
//
// Description:
// Implement the function of obtaining license-related data
//=============================================================


#include "BSGuardSettings.h"
#include "BSLicenseUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"
#include "Misc/Base64.h"
#include "BSCommonDefinition.h"

static FString GetLicenseFilePath()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BSGuardEncryption"));
	if (!Plugin.IsValid())
	{
		return FString();
	}
	return  FPaths::Combine(Plugin->GetContentDir(), TEXT("Lib"),TEXT("site-packages"),TEXT("license"), TEXT("License.bin"));
}

static void DeriveKeyFromPublicKey(TArray<uint8>& OutKey)
{
	FString Pem = FBSLicenseUtils::GetPublicKeyPem();
	FTCHARToUTF8 Convert(*Pem);
	OutKey.SetNum(32);
#if WITH_OPENSSL
	SHA256(reinterpret_cast<const unsigned char*>(Convert.Get()), Convert.Length(), OutKey.GetData());
#else
	FSHA256::HashBuffer(Convert.Get(), Convert.Length(), OutKey.GetData());
#endif
}

bool FBSGuardSettings::ValidateAndSetKey()
{
	bKeyIsValid = false;
	KeyBytes.Empty();

	// The editor is always available by deriving the key from the public key
	DeriveKeyFromPublicKey(KeyBytes);
	if (KeyBytes.Num() != 32)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to derive encryption key from public key."));
		return false;
	}
	
	LicenseFilePath = GetLicenseFilePath();
	if (!LicenseFilePath.IsEmpty())
	{
		FBSLicenseData LicData;
		if (FBSLicenseUtils::LoadLicense(LicenseFilePath, LicData))
		{
			if (!LicData.ExpireDate.IsEmpty())
			{
				FDateTime ExpiryTime;
				if (FDateTime::ParseIso8601(*LicData.ExpireDate, ExpiryTime))
				{
					if (FDateTime::UtcNow() > ExpiryTime)
					{
						UE_LOG(LogTemp, Warning, TEXT("License expired on %s."), *LicData.ExpireDate);
					}
					else
					{
						bKeyIsValid = true;
						KeyExpiration = LicData.ExpireDate;
						return true;
					}
				}
			}
			UE_LOG(LogTemp, Display, TEXT("License checked."));
		}
	}

	return false;
}

const TArray<uint8>& FBSGuardSettings::GetKeyBytes() const
{
	return KeyBytes;
}
