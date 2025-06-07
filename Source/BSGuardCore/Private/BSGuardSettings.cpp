// Fill out your copyright notice in the Description page of Project Settings.


#include "BSGuardSettings.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"
#include "Misc/Base64.h"
#include "BSLicenseUtils.h"

bool UBSGuardSettings::ValidateAndSetKey()
{
       bKeyIsValid = false;
       KeyBytes.Empty();

       // 优先尝试从License文件加载密钥
       if (!LicenseFilePath.IsEmpty())
       {
               FBSLicenseData LicData;
               if (FBSLicenseUtils::LoadLicense(LicenseFilePath, LicData))
               {
                       if (!FBase64::Decode(LicData.SharedKey, KeyBytes) || KeyBytes.Num() != 32)
                       {
                               UE_LOG(LogTemp, Error, TEXT("Invalid SharedKey in license"));
                               return false;
                       }
                       if (!LicData.ExpireDate.IsEmpty())
                       {
                               FDateTime ExpiryTime;
                               if (FDateTime::ParseIso8601(*LicData.ExpireDate, ExpiryTime))
                               {
                                       if (FDateTime::UtcNow() > ExpiryTime)
                                       {
                                               UE_LOG(LogTemp, Error, TEXT("License expired on %s."), *LicData.ExpireDate);
                                               return false;
                                       }
                                       KeyExpiration = LicData.ExpireDate;
                               }
                       }
                       bKeyIsValid = true;
                       UE_LOG(LogTemp, Display, TEXT("License validated successfully."));
                       return true;
               }
               else
               {
                       UE_LOG(LogTemp, Error, TEXT("Failed to load license file: %s"), *LicenseFilePath);
                       // fall through to legacy key input
               }
       }

       if (UserKeyInput.IsEmpty())
       {
               UE_LOG(LogTemp, Warning, TEXT("Encryption key is empty."));
               return false;
       }
	// 假设密钥格式: Base64编码的32字节密钥 + 可选的到期时间（例如追加在后面）。
	FString EncodedKey = UserKeyInput;
	FString ExpirationPart;
	// 简单解析：如果存在分隔符，例如使用 '|' 分隔密钥和时间
	if (UserKeyInput.Contains(TEXT("|")))
	{
		TArray<FString> Parts;
		UserKeyInput.ParseIntoArray(Parts, TEXT("|"));
		if (Parts.Num() >= 2)
		{
			EncodedKey = Parts[0];
			ExpirationPart = Parts[1];
		}
	}
	// 解码Base64获得密钥字节
	TArray<uint8> Decoded;
	if (!FBase64::Decode(EncodedKey, Decoded) || Decoded.Num() != 32)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid encryption key format (expected 32 bytes Base64)."));
		return false;
	}
	// 检查过期时间
	if (!ExpirationPart.IsEmpty())
	{
		// 假设ExpirationPart是一个可解析为DateTime的字符串，例如 "2025-12-31T23:59:59Z"
		FDateTime ExpiryTime;
		if (FDateTime::ParseIso8601(*ExpirationPart, ExpiryTime))
		{
			FDateTime Now = FDateTime::UtcNow();
			if (Now > ExpiryTime)
			{
				UE_LOG(LogTemp, Error, TEXT("Encryption key has expired on %s."), *ExpirationPart);
				return false;
			}
			// 保存过期时间字符串（只读显示用）
			KeyExpiration = ExpirationPart;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid expiration time format. Ignoring expiration check."));
		}
	}

	// 如果需要防止用户篡改Expiration，可以在密钥中加入签名校验，此处略过复杂实现。
	// 例如可以要求UserKeyInput包含一个HMAC或数字签名，用内部公钥验证，以防用户修改过期时间。

	// 密钥通过验证，保存到内存
	KeyBytes = Decoded;
	bKeyIsValid = true;
	UE_LOG(LogTemp, Display, TEXT("Encryption key validated and set. Expiration: %s"), *KeyExpiration);
	return true;
}