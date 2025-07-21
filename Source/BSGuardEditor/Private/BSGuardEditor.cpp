// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSGuardEditor.h"

#include "AssetToolsModule.h"
#include "AssetTypeActions_EncryptedAsset.h"
#include "ContentBrowserModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "BSGuardCrypto.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserDelegates.h"
#include "AssetRegistry/AssetData.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/MessageDialog.h"
#include "Styling/SlateStyleRegistry.h"
#include "UObject/Package.h"
#include "UObject/ObjectSaveContext.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
#include "UObject/SavePackage.h"
#else
#endif


#define LOCTEXT_NAMESPACE "FBSGuardEditorModule"

//***********************************
class FBSGE_AssetActions
{
public:

	static void RegisterAssetActions();
	static void UnregisterAssetActions();

	static void RegisterAssetTypeAction();
	static void UnregisterAssetTypeAction();
private:
	
	static TSharedRef<FExtender> OnExtendContentBrowserAssetMenu(const TArray<FAssetData>& SelectedAssets);
	static void CreateAssetContextMenu(FMenuBuilder& MenuBuilder, const TArray<FAssetData> SelectedAssets);
	static void EncryptSelectedAsset(const FAssetData& AssetData);
	static void DecryptSelectedAsset(const FAssetData& AssetData);

	
	static bool CanMigrateEncryptedAssets();
	static void CreateEncryptedAssetsMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);
	static TSharedRef<FExtender> MakeMigrateEncryptedAssets(const TArray<FAssetData>& SelectedAssets);
	static TSharedRef<FExtender> MakeAssetsAction(const TArray<FAssetData>& SelectedAssets);
private:
	static FDelegateHandle ExtenderHandle;
};


void FBSGE_AssetActions::RegisterAssetActions()
{
	bool ValidKey = FBSGuardCrypto::HasValidKey();
	if (ValidKey)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
		CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FBSGE_AssetActions::OnExtendContentBrowserAssetMenu));
	}

}

void FBSGE_AssetActions::UnregisterAssetActions()
{
	if (!ExtenderHandle.IsValid())
		return;

	FContentBrowserModule& CB = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
	auto& Delegates = CB.GetAllAssetViewContextMenuExtenders();
	Delegates.RemoveAll([&](const FContentBrowserMenuExtender_SelectedAssets& D){return D.GetHandle()==ExtenderHandle;});
}

void FBSGE_AssetActions::RegisterAssetTypeAction()
{
	TArray<TWeakPtr<IAssetTypeActions>> OutAssetTypeActionsList;
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.GetAssetTypeActionsList(OutAssetTypeActionsList);

	for (auto& AssetTypeAction : OutAssetTypeActionsList)
	{
		TSharedPtr<FAssetTypeActions_EncryptedAsset> GuardAssetAction = MakeShareable(new FAssetTypeActions_EncryptedAsset(AssetTypeAction.Pin().ToSharedRef()));
		AssetTools.RegisterAssetTypeActions(GuardAssetAction.ToSharedRef());
	}
	
	// 注册保存包事件，以在资产保存后自动加密
	UPackage::PackageSavedWithContextEvent.AddStatic([](const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext Context)
	{
		if (!BSGEncrypt::IsPackageEncrypted(Package))
		{
			return;
		}
		
		// 当一个资产包保存成功后触发
		if (FBSGuardCrypto::IsEncryptedAssetFile(PackageFileName) == false)
		{
			// 如果存在有效密钥，且当前保存的是未加密文件，将其加密
			if (FBSGuardCrypto::EncryptFile(PackageFileName))
			{
				UE_LOG(LogTemp, Log, TEXT("Encrypted asset on save: %s"), *PackageFileName);
			}
		}
	});
}

void FBSGE_AssetActions::UnregisterAssetTypeAction()
{
	
}

TSharedRef<FExtender> FBSGE_AssetActions::OnExtendContentBrowserAssetMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender(new FExtender());
	Extender->AddMenuExtension(
			"CommonAssetActions", // 在高级操作区之后插入
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateStatic(&FBSGE_AssetActions::CreateAssetContextMenu, SelectedAssets)
		);
	return Extender;
}

void FBSGE_AssetActions::CreateAssetContextMenu(FMenuBuilder& MenuBuilder, const TArray<FAssetData> SelectedAssets)
{
	if (SelectedAssets.Num() == 0) return;
	bool HasEncryptionButton = false;
	bool HasDecryptButton = false;
	TArray<FAssetData> NeedEncryptionFiles;
	TArray<FAssetData> NeedDecryptFiles;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		FString AssetFilePath = AssetData.PackageName.ToString();
		AssetFilePath = FPackageName::LongPackageNameToFilename(AssetFilePath, FPackageName::GetAssetPackageExtension());
		// 检查该资产文件是否已加密
		if (FBSGuardCrypto::IsEncryptedAssetFile(AssetFilePath))
		{
			NeedDecryptFiles.Add(AssetData);
			HasDecryptButton = true;
		}
		else
		{
			NeedEncryptionFiles.Add(AssetData);
			HasEncryptionButton = true;
		}
	}
	if (HasDecryptButton)
	{
		// **解密** 菜单项
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DecryptAssetLabel", "解密资产"),
			LOCTEXT("DecryptAssetTooltip", "使用当前密钥解密此资产文件"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Unlock"),  // 假设使用一个解锁图标
			FUIAction(
				FExecuteAction::CreateLambda([NeedDecryptFiles]()
				{
					for (const FAssetData& AssetData : NeedDecryptFiles)
					{
						UPackage* Package = AssetData.GetPackage();
						BSGEncrypt::MarkPackagePlain(Package);
						const FString FileName =FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
						UPackage::SavePackage(Package, nullptr, RF_Standalone, *FileName);
						Package->MarkPackageDirty();
						DecryptSelectedAsset(AssetData);
					}
					
				}),
				FCanExecuteAction::CreateLambda([]()
				{
					return FBSGuardCrypto::HasValidKey();
				})
			)
		);
	}
	if (HasEncryptionButton)
	{
		// **加密** 菜单项
		MenuBuilder.AddMenuEntry(
			LOCTEXT("EncryptAssetLabel", "加密资产"),
			LOCTEXT("EncryptAssetTooltip", "使用当前密钥加密此资产文件"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Lock"),  // 假设使用一个锁图标
			FUIAction(
				FExecuteAction::CreateLambda([NeedEncryptionFiles]()
				{
					for (const FAssetData& AssetData : NeedEncryptionFiles)
					{
						UObject* AssetObj = AssetData.GetAsset();
						UPackage* Package = AssetObj->GetPackage();
						BSGEncrypt::MarkPackageEncrypted(Package);
						const FString FileName =FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
						UPackage::SavePackage(Package, nullptr, RF_Standalone, *FileName);
						Package->MarkPackageDirty();
						EncryptSelectedAsset(AssetData);
					}
				}),
				FCanExecuteAction::CreateLambda([]()
				{
						return FBSGuardCrypto::HasValidKey();
				})
			)
		);
	}
}

void FBSGE_AssetActions::EncryptSelectedAsset(const FAssetData& AssetData)
{
	// 获取物理文件路径 (.uasset 主文件)
	FString PackageName = AssetData.PackageName.ToString();
	FString AssetFilePath = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

	// 确保资产保存了最新内容
	UPackage* Package = AssetData.GetPackage();
	if (Package)
	{
		Package->FullyLoad();
		bool SaveState = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
		FSavePackageArgs SaveArgs;
		SaveState = UPackage::SavePackage(Package, AssetData.GetAsset(), *AssetFilePath, SaveArgs);
#else
		SaveState = UPackage::SavePackage(Package, nullptr, RF_Public | RF_Standalone, *AssetFilePath, nullptr, nullptr, false, true, SAVE_NoError);
#endif
		if (!SaveState)
		{
			UE_LOG(LogTemp, Error, TEXT("SaveFile failed for asset %s, Encrypt interrupted"), *PackageName);
			return;
		}
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
		if (IFileManager::Get().FileExists(*UbulkPath))
		{
			FBSGuardCrypto::EncryptFile(UbulkPath);
		}
		// 通知用户成功，可在输出日志查看
		UE_LOG(LogTemp, Display, TEXT("Asset %s encrypted."), *PackageName);
		// 刷新内容浏览器显示（改变图标等）
		FContentBrowserModule* CBModule = FModuleManager::GetModulePtr<FContentBrowserModule>("ContentBrowser");
		if (CBModule)
		{
			CBModule->Get().SyncBrowserToAssets({ AssetData });
		}
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("EncryptFailed", "加密资产失败，请检查日志。"));
	}
}

void FBSGE_AssetActions::DecryptSelectedAsset(const FAssetData& AssetData)
{
	FString PackageName = AssetData.PackageName.ToString();
	FString AssetFilePath = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

	// 确保资产保存了最新内容
	UPackage* Package = AssetData.GetPackage();
	if (Package)
	{
		Package->FullyLoad();
		bool SaveState = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
		FSavePackageArgs SaveArgs;
		SaveState = UPackage::SavePackage(Package, AssetData.GetAsset(), *AssetFilePath, SaveArgs);
#else
		SaveState = UPackage::SavePackage(Package, nullptr, RF_Public | RF_Standalone, *AssetFilePath, nullptr, nullptr, false, true, SAVE_NoError);
#endif
		if (!SaveState)
		{
			UE_LOG(LogTemp, Error, TEXT("SaveFile failed for asset %s, Decrypt interrupted"), *PackageName);
			return;
		}
	}
	
	// 执行解密
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


FDelegateHandle FBSGE_AssetActions::ExtenderHandle;
//***********************************

void FBSGuardEditorModule::StartupModule()
{
	FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("BSGuardEncryption"))->GetBaseDir() / TEXT("Resources");
	FSlateStyleSet* StyleSet = new FSlateStyleSet("GuardEncryptionStyle");
	StyleSet->SetContentRoot(ContentDir);
	StyleSet->Set("GuardEncryption.LockIcon16", new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("LockIcon16.png")), FVector2D(16,16)));
	StyleSet->Set("GuardEncryption.LockIcon64", new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("LockIcon64.png")), FVector2D(64,64)));
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
	
	
	// 注册内容浏览器菜单扩展
	FBSGE_AssetActions::RegisterAssetActions();
	
	// 注册加密AssetTypeAction
	FBSGE_AssetActions::RegisterAssetTypeAction();

}

void FBSGuardEditorModule::ShutdownModule()
{
	FBSGE_AssetActions::UnregisterAssetActions();
	FBSGE_AssetActions::UnregisterAssetTypeAction();
}




#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBSGuardEditorModule, BSGuardEditor)