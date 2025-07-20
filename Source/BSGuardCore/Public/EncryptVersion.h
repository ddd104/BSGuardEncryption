#pragma once
#include "CoreMinimal.h"
#include "EncryptVersion.generated.h"


UCLASS()
class UBSGEncryptTag : public UAssetUserData
{
	GENERATED_BODY()

	virtual void Serialize(FArchive& Ar) override;
};
UCLASS()
class UBSGPlainTag : public UAssetUserData
{
	GENERATED_BODY()

	virtual void Serialize(FArchive& Ar) override;
};

namespace BSGEncrypt
{
	BSGUARDCORE_API  void MarkPackageEncrypted(UPackage* Pkg);

	BSGUARDCORE_API  void MarkPackagePlain(UPackage* Pkg);
	
	BSGUARDCORE_API  bool IsPackageEncrypted(UPackage* Pkg);

	BSGUARDCORE_API  bool IsObjectEncrypted(UObject* Obj);
}