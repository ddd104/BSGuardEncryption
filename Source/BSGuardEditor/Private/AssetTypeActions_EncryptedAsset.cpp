#include "AssetTypeActions_EncryptedAsset.h"

#include "BSGuardCrypto.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<class SWidget> FAssetTypeActions_EncryptedAsset::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	FString AssetFilePath = FPackageName::LongPackageNameToFilename(AssetData.PackageName.ToString(), FPackageName::GetAssetPackageExtension());
	/*if (FBSGuardCrypto::IsEncryptedAssetFile(AssetFilePath))
	{
		return FSlateStyleRegistry::FindSlateStyle("GuardEncryptionStyle")->GetBrush("GuardEncryption.LockIcon16");
	}*/
	return nullptr;
}
