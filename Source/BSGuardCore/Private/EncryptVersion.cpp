#include "EncryptVersion.h"


struct FBSGEncryptVersion
{
	enum class EBSGEncryptVer : int32
	{
		Plain      = 0,
		Encrypted  = 1,
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
	
	static const FGuid GUID;
};

const FGuid FBSGEncryptVersion::GUID(0x6E4DE065, 0x1F87468C, 0xA5F1E4D3, 0xDC97B2E9);
static FCustomVersionRegistration GBSGEncryptVer(
	FBSGEncryptVersion::GUID,
	(int32)FBSGEncryptVersion::EBSGEncryptVer::LatestVersion,
	TEXT("BSGE_EncryptTag"));


namespace BSGEncrypt
{
	void MarkPackageEncrypted(UPackage* Pkg)
	{
		if (!Pkg) return;


		UObject* MainAsset = Pkg->FindAssetInPackage();

		if (!MainAsset) return;

		if (IInterface_AssetUserData* AUDOwner = Cast<IInterface_AssetUserData>(MainAsset))
		{
			AUDOwner->AddAssetUserDataOfClass(UBSGEncryptTag::StaticClass());
			Pkg->MarkPackageDirty();
			MainAsset->Modify();
		}
	}

	void MarkPackagePlain(UPackage* Pkg)
	{
		if (!Pkg) return;
		
		UObject* MainAsset = Pkg->FindAssetInPackage();

		if (!MainAsset) return;
		
		if (IInterface_AssetUserData* AUDOwner = Cast<IInterface_AssetUserData>(MainAsset))
		{
			AUDOwner->AddAssetUserDataOfClass(UBSGPlainTag::StaticClass());
			Pkg->MarkPackageDirty();
			MainAsset->Modify();
		}
	}

	bool IsPackageEncrypted(UPackage* Pkg)
	{
		if (!Pkg) return false;
		
		/*const int32 Ver = Pkg->GetLinkerCustomVersion(BSGE_ENCRYPT_VER_GUID);

		return Ver == static_cast<int32>(EBSGEncryptVer::Encrypted);*/
		UObject* MainAsset = Pkg->FindAssetInPackage();
		if (IInterface_AssetUserData* AUDOwner = Cast<IInterface_AssetUserData>(MainAsset))
		{
			bool bEncrypted = AUDOwner->GetAssetUserDataOfClass(UBSGEncryptTag::StaticClass()) != nullptr;
			bool bPlain = AUDOwner->GetAssetUserDataOfClass(UBSGPlainTag::StaticClass())!= nullptr;
			return bEncrypted;
		}
		return false;
	}

	bool IsObjectEncrypted(UObject* Obj)
	{
		if (IInterface_AssetUserData* AUDOwner = Cast<IInterface_AssetUserData>(Obj))
		{
			bool bEncrypted = AUDOwner->GetAssetUserDataOfClass(UBSGEncryptTag::StaticClass()) != nullptr;
			bool bPlain = AUDOwner->GetAssetUserDataOfClass(UBSGPlainTag::StaticClass())!= nullptr;
			return bEncrypted;
		}
		return false;
	}
}


void UBSGEncryptTag::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FBSGEncryptVersion::GUID);
	Ar.SetCustomVersion(FBSGEncryptVersion::GUID, (int32)FBSGEncryptVersion::EBSGEncryptVer::Encrypted, TEXT("BSGE_EncryptTag"));
}

void UBSGPlainTag::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FBSGEncryptVersion::GUID);
	Ar.SetCustomVersion(FBSGEncryptVersion::GUID, (int32)FBSGEncryptVersion::EBSGEncryptVer::Plain, TEXT("BSGE_EncryptTag"));
}
