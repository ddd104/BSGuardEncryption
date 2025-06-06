// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSGuardEditor.h"
#include "ContentBrowserModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "BSGuardCrypto.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetData.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/MessageDialog.h"
#include "Styling/SlateStyleRegistry.h"
#include "UObject/Package.h"
#include "UObject/ObjectSaveContext.h"


#define LOCTEXT_NAMESPACE "FBSGuardEditorModule"

void FBSGuardEditorModule::StartupModule()
{
	FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("BSGuardEncryption"))->GetBaseDir() / TEXT("Resources");
	FSlateStyleSet* StyleSet = new FSlateStyleSet("GuardEncryptionStyle");
	StyleSet->SetContentRoot(ContentDir);
	StyleSet->Set("GuardEncryption.LockIcon16", new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("LockIcon16.png")), FVector2D(16,16)));
	StyleSet->Set("GuardEncryption.LockIcon64", new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("LockIcon64.png")), FVector2D(64,64)));
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
	
	// 注册内容浏览器菜单扩展
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FBSGuardEditorModule::OnExtendContentBrowserAssetMenu));

	// 注册保存包事件，以在资产保存后自动加密
	UPackage::PackageSavedWithContextEvent.AddStatic([](const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext Context)
	{
		// 当一个资产包保存成功后触发
		if (FBSGuardCrypto::HasValidKey() && FBSGuardCrypto::IsEncryptedAssetFile(PackageFileName) == false)
		{
			// 如果存在有效密钥，且当前保存的是未加密文件，将其加密
			if (FBSGuardCrypto::EncryptFile(PackageFileName))
			{
				UE_LOG(LogTemp, Log, TEXT("Encrypted asset on save: %s"), *PackageFileName);
			}
		}
	});

	// TODO: 可在此注册项目设置面板 (通过ISettingsModule) 以提供密钥输入UI。
	// 例如:
	// ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	// if (SettingsModule) {
	//     SettingsModule->RegisterSettings("Project", "Plugins", "GuardEncryption",
	//         LOCTEXT("GuardSettingsName", "Guard Encryption"),
	//         LOCTEXT("GuardSettingsDescription", "Configure GuardEncryption Plugin"),
	//         GetMutableDefault<UGuardSettings>());
	// }
}

void FBSGuardEditorModule::ShutdownModule()
{
	
}

TSharedRef<FExtender> FBSGuardEditorModule::OnExtendContentBrowserAssetMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender(new FExtender());
	// 仅当选择了一个资产时扩展（批量处理可扩展，此处以单个资产示例）
	if (SelectedAssets.Num() == 1)
	{
		Extender->AddMenuExtension(
			"CommonAssetActions", // 在高级操作区之后插入
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateStatic(&FBSGuardEditorModule::CreateAssetContextMenu, SelectedAssets)
		);
	}
	return Extender;
}

void FBSGuardEditorModule::CreateAssetContextMenu(FMenuBuilder& MenuBuilder, const TArray<FAssetData> SelectedAssets)
{
	if (SelectedAssets.Num() == 0) return;
	const FAssetData& AssetData = SelectedAssets[0];
	FString AssetFilePath = AssetData.PackageName.ToString();
	AssetFilePath = FPackageName::LongPackageNameToFilename(AssetFilePath, FPackageName::GetAssetPackageExtension());  // 转换为.uasset文件路径

	// 检查该资产文件是否已加密
	bool bIsEncrypted = FBSGuardCrypto::IsEncryptedAssetFile(AssetFilePath);
	if (bIsEncrypted)
	{
		// **解密** 菜单项
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DecryptAssetLabel", "解密资产"),
			LOCTEXT("DecryptAssetTooltip", "使用当前密钥解密此资产文件"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Unlock"),  // 假设使用一个解锁图标
			FUIAction(
				FExecuteAction::CreateStatic(TIdentity<TDelegate<void()>::RetValType(*)(std::decay_t<const FAssetData&>)>::Type(
					                             &FBSGuardEditorModule::DecryptSelectedAsset), AssetData),
				FCanExecuteAction::CreateLambda([AssetFilePath]()
				{
					// 仅当密钥有效时允许解密
					return FBSGuardCrypto::HasValidKey();
				})
			)
		);
		// 如有需要，禁用导出等操作
		// （例如可以在Extender添加前移除Export菜单，此处简化为注释）
		// MenuBuilder.RemoveMenuEntry(FName("ExportAsset"));  // 假设可以通过名称移除
	}
	else
	{
		// **加密** 菜单项
		MenuBuilder.AddMenuEntry(
			LOCTEXT("EncryptAssetLabel", "加密资产"),
			LOCTEXT("EncryptAssetTooltip", "使用当前密钥加密此资产文件"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Lock"),  // 假设使用一个锁图标
			FUIAction(
				FExecuteAction::CreateStatic(TIdentity<TDelegate<void()>::RetValType(*)(std::decay_t<const FAssetData&>)>::Type(
					                             &FBSGuardEditorModule::EncryptSelectedAsset), AssetData),
				FCanExecuteAction::CreateLambda([AssetFilePath]()
				{
					// 需要有有效密钥，且文件未加密
					return FBSGuardCrypto::HasValidKey() && !FBSGuardCrypto::IsEncryptedAssetFile(AssetFilePath);
				})
			)
		);
	}
}

void FBSGuardEditorModule::EncryptSelectedAsset(const FAssetData& AssetData)
{
	// 获取物理文件路径 (.uasset 主文件)
	FString PackageName = AssetData.PackageName.ToString();
	FString AssetFilePath = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	if (!FBSGuardCrypto::HasValidKey())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoKeyError", "密钥未设置或无效，无法加密资产。"));
		return;
	}
	// 确保资产保存了最新内容
	UPackage* Package = AssetData.GetPackage();
	if (Package)
	{
		Package->FullyLoad();
		// 保存Package到磁盘，以更新.uasset文件
		FString Error;
		UPackage::SavePackage(Package, nullptr, RF_Public | RF_Standalone, *AssetFilePath, nullptr, nullptr, false, true, SAVE_NoError);
	}
	// 执行加密
	if (FBSGuardCrypto::EncryptFile(AssetFilePath))
	{
		// 如资产还有.euxp或.ubulk文件，也执行加密
		FString UexpPath = FPaths::ChangeExtension(AssetFilePath, TEXT(".uexp"));
		if (IFileManager::Get().FileExists(*UexpPath))
		{
			FBSGuardCrypto::EncryptFile(UexpPath);
		}
		FString UbulkPath = FPaths::ChangeExtension(AssetFilePath, TEXT(".ubulk"));
		if (IFileManager::

		Get().FileExists(*UbulkPath))
		{
			FBSGuardCrypto::EncryptFile(UbulkPath);
		}
		// 通知用户成功，可在输出日志查看
		UE_LOG(LogTemp, Display, TEXT("Asset %s encrypted."), *PackageName);
		// 刷新内容浏览器显示（改变图标等）
		FContentBrowserModule* CBModule = FModuleManager::GetModulePtr<FContentBrowserModule>("ContentBrowser");
		if (CBModule)
		{
			CBModule->Get().SyncBrowserToAssets({ AssetData }); // 简单做一下刷新
		}
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("EncryptFailed", "加密资产失败，请检查日志。"));
	}
}

void FBSGuardEditorModule::DecryptSelectedAsset(const FAssetData& AssetData)
{
	FString PackageName = AssetData.PackageName.ToString();
	FString AssetFilePath = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	if (!FBSGuardCrypto::HasValidKey())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoKeyError", "密钥未设置或无效，无法解密资产。"));
		return;
	}
	// 执行解密（需注意解密后文件即为明文，应妥善处理）
	if (FBSGuardCrypto::DecryptFile(AssetFilePath))
	{
		FString UexpPath = FPaths::ChangeExtension(AssetFilePath, TEXT(".uexp"));
		if (IFileManager::Get().FileExists(*UexpPath))
		{
			FBSGuardCrypto::DecryptFile(UexpPath);
		}
		FString UbulkPath = FPaths::ChangeExtension(AssetFilePath, TEXT(".ubulk"));
		if (IFileManager::Get().FileExists(*UbulkPath))
		{
			FBSGuardCrypto::DecryptFile(UbulkPath);
		}
		UE_LOG(LogTemp, Display, TEXT("Asset %s decrypted."), *PackageName);
		// 重新加载资产，以反映明文（例如此时可以打开查看）
		if (UPackage* Package = AssetData.GetPackage())
		{
			Package->FullyLoad();
		}
		// 刷新浏览器
		FContentBrowserModule* CBModule = FModuleManager::GetModulePtr<FContentBrowserModule>("ContentBrowser");
		if (CBModule)
		{
			CBModule->Get().SyncBrowserToAssets({ AssetData });
		}
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DecryptFailed", "解密资产失败，请检查密钥或日志。"));
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBSGuardEditorModule, BSGuardEditor)