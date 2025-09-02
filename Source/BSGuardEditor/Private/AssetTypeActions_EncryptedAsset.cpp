#include "AssetTypeActions_EncryptedAsset.h"

#include "BSGuardCrypto.h"

TSharedPtr<class SWidget> FAssetTypeActions_EncryptedAsset::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	const FString& PackageExt = FBSGuardCrypto::ChooseHeaderExt(AssetData);
	FString AssetFilePath = FPackageName::LongPackageNameToFilename(AssetData.PackageName.ToString(), PackageExt);
	return nullptr;
}

FAssetTypeActions_EncryptedAsset::FAssetTypeActions_EncryptedAsset(const TSharedRef<IAssetTypeActions>& InInner): Inner(InInner)
{
	
}

void FAssetTypeActions_EncryptedAsset::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> Host)
{
	UE_LOG(LogTemp, Display, TEXT("FAssetTypeActions_EncryptedAsset::OpenAssetEditor"));
	TArray<UObject*> MakeOpenObjects;
	for (UObject* Asset : InObjects)
	{
		const FString& PackageExt = FBSGuardCrypto::ChooseHeaderExt(Asset);
		FString AssetPath = FPackageName::LongPackageNameToFilename(Asset->GetPackage()->GetName(), PackageExt);
        
		FILE* File = nullptr;
		fopen_s(&File, TCHAR_TO_ANSI(*AssetPath), "rb");

		if (!File)
		{
			if (!BSGEncrypt::IsObjectEncrypted(Asset))
			{
				MakeOpenObjects.Add(Asset);
			}
			else
			{
				if (IsOpenAllowed())
				{
					MakeOpenObjects.Add(Asset);
				}
			}
			continue;
		}
		char Magic[4];
		fread(Magic, 1, 4, File);
		fclose(File);

		const bool bIsEncrypted = FMemory::Memcmp(Magic, "BSGE", 4) == 0;
		if (!bIsEncrypted)
		{
			MakeOpenObjects.Add(Asset);
			continue;
		}
		if (IsOpenAllowed())
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Failed to pass verification, refused to open this asset."));
			//MakeOpenObjects.Add(Asset);
		}
	}
	Inner->OpenAssetEditor(MakeOpenObjects, Host);
}

FAssetTypeActions_EncryptedAsset::~FAssetTypeActions_EncryptedAsset()
{
}

bool FAssetTypeActions_EncryptedAsset::HasActions(const TArray<UObject*>& InObjects) const
{
	return Inner->HasActions(InObjects);
}

void FAssetTypeActions_EncryptedAsset::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	Inner->GetActions(InObjects, MenuBuilder);
}

void FAssetTypeActions_EncryptedAsset::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	Inner->GetActions(InObjects, Section);
}

TArray<FAssetData> FAssetTypeActions_EncryptedAsset::GetValidAssetsForPreviewOrEdit(
	TArrayView<const FAssetData> InAssetDatas, bool bIsPreview)
{
	return Inner->GetValidAssetsForPreviewOrEdit(InAssetDatas, bIsPreview);
}

bool FAssetTypeActions_EncryptedAsset::CanMerge() const
{
	return Inner->CanMerge();
}

void FAssetTypeActions_EncryptedAsset::Merge(UObject* InObject)
{
	Inner->Merge(InObject);
}

void FAssetTypeActions_EncryptedAsset::Merge(UObject* BaseAsset, UObject* RemoteAsset, UObject* LocalAsset,
	const FOnMergeResolved& ResolutionCallback)
{
	Inner->Merge(BaseAsset, RemoteAsset, LocalAsset, ResolutionCallback);
}

FString FAssetTypeActions_EncryptedAsset::GetObjectDisplayName(UObject* Object) const
{
	return Inner->GetObjectDisplayName(Object);
}

const TArray<FText>& FAssetTypeActions_EncryptedAsset::GetSubMenus() const
{
	return Inner->GetSubMenus();
}

void FAssetTypeActions_EncryptedAsset::PerformAssetDiff(UObject* OldAsset, UObject* NewAsset,
	const FRevisionInfo& OldRevision, const FRevisionInfo& NewRevision) const
{
	Inner->PerformAssetDiff(OldAsset, NewAsset, OldRevision, NewRevision);
}

UThumbnailInfo* FAssetTypeActions_EncryptedAsset::GetThumbnailInfo(UObject* Asset) const
{
	return Inner->GetThumbnailInfo(Asset);
}

EThumbnailPrimType FAssetTypeActions_EncryptedAsset::GetDefaultThumbnailPrimitiveType(UObject* Asset) const
{
	return Inner->GetDefaultThumbnailPrimitiveType(Asset);
}

FText FAssetTypeActions_EncryptedAsset::GetAssetDescription(const FAssetData& AssetData) const
{
	return Inner->GetAssetDescription(AssetData);
}

bool FAssetTypeActions_EncryptedAsset::IsImportedAsset() const
{
	return Inner->IsImportedAsset();
}

void FAssetTypeActions_EncryptedAsset::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets,
	TArray<FString>& OutSourceFilePaths) const
{
	Inner->GetResolvedSourceFilePaths(TypeAssets, OutSourceFilePaths);
}

void FAssetTypeActions_EncryptedAsset::GetSourceFileLabels(const TArray<UObject*>& TypeAssets,
	TArray<FString>& OutSourceFileLabels) const
{
	Inner->GetSourceFileLabels(TypeAssets, OutSourceFileLabels);
}

void FAssetTypeActions_EncryptedAsset::BuildBackendFilter(FARFilter& InFilter)
{
	Inner->BuildBackendFilter(InFilter);
}

FText FAssetTypeActions_EncryptedAsset::GetDisplayNameFromAssetData(const FAssetData& AssetData) const
{
	return Inner->GetDisplayNameFromAssetData(AssetData);
}

bool FAssetTypeActions_EncryptedAsset::CanFilter()
{
	return Inner->CanFilter();
}

FText FAssetTypeActions_EncryptedAsset::GetName() const
{
	return Inner->GetName();
}

UClass* FAssetTypeActions_EncryptedAsset::GetSupportedClass() const
{
	return Inner->GetSupportedClass();
}

FColor FAssetTypeActions_EncryptedAsset::GetTypeColor() const
{
	return Inner->GetTypeColor();
}

bool FAssetTypeActions_EncryptedAsset::AssetsActivatedOverride(const TArray<UObject*>& InObjects,
	EAssetTypeActivationMethod::Type ActivationType)
{
	return Inner->AssetsActivatedOverride(InObjects, ActivationType);
}

bool FAssetTypeActions_EncryptedAsset::CanLocalize() const
{
	return Inner->CanLocalize();
}

uint32 FAssetTypeActions_EncryptedAsset::GetCategories()
{
	return Inner->GetCategories();
}

bool FAssetTypeActions_EncryptedAsset::ShouldForceWorldCentric()
{
	
	return Inner->ShouldForceWorldCentric();
}

bool FAssetTypeActions_EncryptedAsset::IsOpenAllowed() const
{
	return FBSGuardCrypto::HasValidKey();
}

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 27

#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0
FName FAssetTypeActions_EncryptedAsset::GetFilterName() const
{
	return Inner->GetFilterName();
}

FTopLevelAssetPath FAssetTypeActions_EncryptedAsset::GetClassPathName() const
{
	return Inner->GetClassPathName();
}

bool FAssetTypeActions_EncryptedAsset::SupportsOpenedMethod(const EAssetTypeActivationOpenedMethod OpenedMethod) const
{
	return Inner->SupportsOpenedMethod(OpenedMethod);
}

const FSlateBrush* FAssetTypeActions_EncryptedAsset::GetThumbnailBrush(const FAssetData& InAssetData,
	const FName InClassName) const
{
	return Inner->GetThumbnailBrush(InAssetData, InClassName);
}

const FSlateBrush* FAssetTypeActions_EncryptedAsset::GetIconBrush(const FAssetData& InAssetData,
	const FName InClassName) const
{
	return Inner->GetIconBrush(InAssetData, InClassName);
}

bool FAssetTypeActions_EncryptedAsset::CanRename(const FAssetData& InAsset, FText* OutErrorMsg) const
{
	return Inner->CanRename(InAsset, OutErrorMsg);
}

bool FAssetTypeActions_EncryptedAsset::CanDuplicate(const FAssetData& InAsset, FText* OutErrorMsg) const
{
	return Inner->CanDuplicate(InAsset, OutErrorMsg);
}

bool FAssetTypeActions_EncryptedAsset::IsAssetDefinitionInDisguise() const
{
	return Inner->IsAssetDefinitionInDisguise();
}

bool FAssetTypeActions_EncryptedAsset::ShouldCallGetActions() const
{
	return Inner->ShouldCallGetActions();
}

void FAssetTypeActions_EncryptedAsset::OpenAssetEditor(const TArray<UObject*>& InObjects,
	const EAssetTypeActivationOpenedMethod OpenedMethod, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	UE_LOG(LogTemp, Display, TEXT("FAssetTypeActions_EncryptedAsset::OpenAssetEditor"));
	TArray<UObject*> MakeOpenObjects;
	for (UObject* Asset : InObjects)
	{
		const FString& PackageExt = FBSGuardCrypto::ChooseHeaderExt(Asset);
		FString AssetPath = FPackageName::LongPackageNameToFilename(Asset->GetPackage()->GetName(), PackageExt);
        
		FILE* File = nullptr;
		fopen_s(&File, TCHAR_TO_ANSI(*AssetPath), "rb");

		if (!File)
		{
			if (!BSGEncrypt::IsObjectEncrypted(Asset))
			{
				MakeOpenObjects.Add(Asset);
			}
			else
			{
				if (IsOpenAllowed())
				{
					MakeOpenObjects.Add(Asset);
				}
			}
			continue;
		}
		char Magic[4];
		fread(Magic, 1, 4, File);
		fclose(File);

		const bool bIsEncrypted = FMemory::Memcmp(Magic, "BSGE", 4) == 0;
		if (!bIsEncrypted)
		{
			MakeOpenObjects.Add(Asset);
			continue;
		}
		if (IsOpenAllowed())
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Failed to pass verification, refused to open this asset."));
			MakeOpenObjects.Add(Asset);
		}
	}
	Inner->OpenAssetEditor(MakeOpenObjects, OpenedMethod, EditWithinLevelEditor);
}
#endif
