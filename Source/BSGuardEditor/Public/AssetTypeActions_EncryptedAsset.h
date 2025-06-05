#pragma once
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_EncryptedAsset : public FAssetTypeActions_Base
{
public:
	// 让这个Action适用于所有UObject或特定类，这里为示例假设纹理
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "EncryptedAsset", "Encrypted Asset"); }
	virtual FColor GetTypeColor() const override { return FColor::White; }
	virtual UClass* GetSupportedClass() const override { return UTexture::StaticClass(); } // 示例针对纹理类
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return false; }
	// 缩略图覆盖：如果资产文件是加密的，返回锁图标
	virtual TSharedPtr<class SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;
};

