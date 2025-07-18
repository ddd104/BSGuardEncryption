#pragma once

const FGuid BSGE_ENCRYPT_VER_GUID(0x6E4DE065, 0x1F87468C, 0xA5F1E4D3, 0xDC97B2E9);
enum class EBSGEncryptVer  : int32 { Plain = 0, Encrypted = 1 };

// 注册到引擎（一次性）
static FCustomVersionRegistration GBSGEncryptVer(
		BSGE_ENCRYPT_VER_GUID,
		(int32)EBSGEncryptVer::Encrypted,
		TEXT("BSGE_EncryptTag"));



namespace BSGEncrypt
{
	void MarkPackageEncrypted(UPackage* Pkg);

	void MarkPackagePlain(UPackage* Pkg);
	
	bool IsPackageEncrypted(UPackage* Pkg);
}