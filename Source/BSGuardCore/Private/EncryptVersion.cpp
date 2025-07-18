#include "EncryptVersion.h"

void BSGEncrypt::MarkPackageEncrypted(UPackage* Pkg)
{
	FArchive Ar;
	Pkg->Serialize(Ar);
	Ar.SetCustomVersion(BSGE_ENCRYPT_VER_GUID,
		(int32)EBSGEncryptVer::Encrypted,
		TEXT("BSGE_EncryptTag"));
}

void BSGEncrypt::MarkPackagePlain(UPackage* Pkg)
{
	FArchive Ar;
	Pkg->Serialize(Ar);
	Ar.SetCustomVersion(BSGE_ENCRYPT_VER_GUID,
		(int32)EBSGEncryptVer::Plain,
		TEXT("BSGE_EncryptTag"));
}

bool BSGEncrypt::IsPackageEncrypted(UPackage* Pkg)
{
	FArchive Ar;
	Pkg->Serialize(Ar);
	const FCustomVersionContainer& CVC = Ar.GetCustomVersions();
	return CVC.GetVersion(BSGE_ENCRYPT_VER_GUID)->Version == (int32)EBSGEncryptVer::Encrypted;
}
