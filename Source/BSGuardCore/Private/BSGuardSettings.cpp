// Fill out your copyright notice in the Description page of Project Settings.


#include "BSGuardSettings.h"
#include "BSLicenseUtils..h"
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
	return  FPaths::Combine(Plugin->GetBaseDir(), TEXT("license"), TEXT("License.license"));
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

bool UBSGuardSettings::ValidateAndSetKey()
{
	bKeyIsValid = false;
	KeyBytes.Empty();

	// 通过公钥派生密钥，编辑器始终可用
	DeriveKeyFromPublicKey(KeyBytes);
	if (KeyBytes.Num() != 32)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to derive encryption key from public key."));
		return false;
	}

	// 如存在 License 文件，仅用于校验过期等信息
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
						KeyExpiration = LicData.ExpireDate;
					}
				}
			}
			UE_LOG(LogTemp, Display, TEXT("License checked."));
		}
	}

	// 密钥通过验证，保存到内存
	bKeyIsValid = true;
	return true;
}

const TArray<uint8>& UBSGuardSettings::GetKeyBytes() const
{
	return KeyBytes;
}
