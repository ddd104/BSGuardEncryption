// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FBSGuardEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
private:
	// 菜单扩展处理函数
	static TSharedRef<FExtender> OnExtendContentBrowserAssetMenu(const TArray<FAssetData>& SelectedAssets);
	static void CreateAssetContextMenu(FMenuBuilder& MenuBuilder, const TArray<FAssetData> SelectedAssets);

	// 命令执行函数
	static void EncryptSelectedAsset(const FAssetData& AssetData);
	static void DecryptSelectedAsset(const FAssetData& AssetData);
};
