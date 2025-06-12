#pragma once
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_EncryptedAsset : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_EncryptedAsset(const TSharedRef<IAssetTypeActions>& InInner);

	/** —— 关键拦截点 —— */
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> Host) override;

public:
	virtual ~FAssetTypeActions_EncryptedAsset() override;

	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;

	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;

	virtual void GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section) override;
	virtual bool IsAssetDefinitionInDisguise() const override;
	virtual bool ShouldCallGetActions() const override;

	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, const EAssetTypeActivationOpenedMethod OpenedMethod,
	                             TSharedPtr<IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;

	virtual bool CanRename(const FAssetData& InAsset, FText* OutErrorMsg) const override;

	virtual bool CanDuplicate(const FAssetData& InAsset, FText* OutErrorMsg) const override;

	virtual TArray<FAssetData> GetValidAssetsForPreviewOrEdit(TArrayView<const FAssetData> InAssetDatas,
	                                                          bool bIsPreview) override;
	virtual bool CanMerge() const override;
	virtual void Merge(UObject* InObject) override;

	virtual void Merge(UObject* BaseAsset, UObject* RemoteAsset, UObject* LocalAsset,
	                   const FOnMergeResolved& ResolutionCallback) override;
	virtual FString GetObjectDisplayName(UObject* Object) const override;
	virtual const TArray<FText>& GetSubMenus() const override;

	virtual void PerformAssetDiff(UObject* OldAsset, UObject* NewAsset, const FRevisionInfo& OldRevision,
	                              const FRevisionInfo& NewRevision) const override;
	virtual UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	virtual EThumbnailPrimType GetDefaultThumbnailPrimitiveType(UObject* Asset) const override;
	virtual TSharedPtr<class SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
	virtual bool IsImportedAsset() const override;

	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets,
	                                        TArray<FString>& OutSourceFilePaths) const override;

	virtual void GetSourceFileLabels(const TArray<UObject*>& TypeAssets,
	                                 TArray<FString>& OutSourceFileLabels) const override;

	virtual void BuildBackendFilter(FARFilter& InFilter) override;
	virtual FText GetDisplayNameFromAssetData(const FAssetData& AssetData) const override;

	virtual bool SupportsOpenedMethod(const EAssetTypeActivationOpenedMethod OpenedMethod) const override;

	virtual const FSlateBrush* GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const override;

	virtual const FSlateBrush* GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const override;
	virtual bool CanFilter();
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;

	virtual bool AssetsActivatedOverride(const TArray<UObject*>& InObjects,
	                                     EAssetTypeActivationMethod::Type ActivationType) override;
	virtual FName GetFilterName() const override;
	virtual bool CanLocalize() const override;
	virtual uint32 GetCategories() override;
	virtual bool ShouldForceWorldCentric() override;
	virtual FTopLevelAssetPath GetClassPathName() const override;

private:
	bool IsOpenAllowed() const;

private:
	TSharedRef<IAssetTypeActions> Inner;
};

