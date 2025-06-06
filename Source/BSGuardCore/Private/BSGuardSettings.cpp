// Fill out your copyright notice in the Description page of Project Settings.


#include "BSGuardSettings.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"
#include "Misc/Base64.h"

bool UBSGuardSettings::ValidateAndSetKey()
{
	bKeyIsValid = false;
	KeyBytes.Empty();

	if (UserKeyInput.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Encryption key is empty."));
		return false;
	}
	// 假设密钥格式: Base64编码的32字节密钥 + 可选的到期时间（例如追加在后面）。
    FString EncodedKey = UserKeyInput;
    FString ExpirationPart;
    FString UserIdPart;
    // 格式: Base64Key|Expiration|UserId  (后两项可省略)
    if (UserKeyInput.Contains(TEXT("|")))
    {
            TArray<FString> Parts;
            UserKeyInput.ParseIntoArray(Parts, TEXT("|"));
            if (Parts.Num() >= 2)
            {
                    EncodedKey = Parts[0];
                    ExpirationPart = Parts[1];
            }
            if (Parts.Num() >= 3)
            {
                    UserIdPart = Parts[2];
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

        if (!UserIdPart.IsEmpty())
        {
                UserId = UserIdPart;
        }
        else
        {
                UserId = TEXT("DefaultUser");
        }

	// 如果需要防止用户篡改Expiration，可以在密钥中加入签名校验，此处略过复杂实现。
	// 例如可以要求UserKeyInput包含一个HMAC或数字签名，用内部公钥验证，以防用户修改过期时间。

	// 密钥通过验证，保存到内存
	KeyBytes = Decoded;
	bKeyIsValid = true;
	UE_LOG(LogTemp, Display, TEXT("Encryption key validated and set. Expiration: %s"), *KeyExpiration);
	return true;
}