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
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 27

#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0 && ENGINE_MINOR_VERSION < 6
#include "UObject/ObjectSaveContext.h"
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
#include "UObject/SavePackage.h"
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
private:
	static FDelegateHandle ExtenderHandle;
	static TArray<TSharedPtr<FAssetTypeActions_EncryptedAsset>> GuardAssetActions;
};

TArray<TSharedPtr<FAssetTypeActions_EncryptedAsset>> FBSGE_AssetActions::GuardAssetActions;

void FBSGE_AssetActions::RegisterAssetActions()
{
	bool ValidKey = FBSGuardCrypto::HasValidKey();
	if (ValidKey)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
		CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FBSGE_AssetActions::OnExtendContentBrowserAssetMenu));
		ExtenderHandle = CBMenuExtenderDelegates.Last().GetHandle();
		
		//CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FBSGE_AssetActions::MakeMigrateEncryptedAssets));
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
		GuardAssetActions.Emplace(GuardAssetAction);
		AssetTools.RegisterAssetTypeActions(GuardAssetAction.ToSharedRef());
	}

	FBSGuardCrypto::bRetBoolDelegate.BindLambda(
		[]()->bool
		{
			IAssetTools& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			
			for (auto& AssetTypeAction : GuardAssetActions)
			{
				UClass* TargetClass = AssetTypeAction->GetSupportedClass();

				TSharedPtr<IAssetTypeActions> Active = Tools.GetAssetTypeActionsForClass(TargetClass).Pin();

				if (Active == AssetTypeAction)          // 指针相等 ⇒ 当前引擎用的就是这一条
				{
					return true;
				}
			}
			// *指针相等* ⇒ 说明当前引擎正使用你的实现
			return false;
		}
	);
	
	// 注册保存包事件，以在资产保存后自动加密
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 27
	UPackage::PackageSavedEvent.AddStatic([](const FString& PackageFileName, UObject* Object)
		{
			check(Object)
			UPackage* Package = Object->GetPackage();
			if (!FBSGuardCrypto::ShouldEncryptAsset(PackageFileName))
			{
				return;
			}
			if (!BSGEncrypt::IsPackageEncrypted(Package))
			{
				return;
			}
		
			// 当一个资产包保存成功后触发
			if (FBSGuardCrypto::IsEncryptedAssetFile(PackageFileName) == false)
			{
				Package->SetPackageFlags(PKG_DisallowExport);
				// 如果存在有效密钥，且当前保存的是未加密文件，将其加密
				if (FBSGuardCrypto::EncryptFile(PackageFileName))
				{
					UE_LOG(LogTemp, Log, TEXT("Encrypted asset on save: %s, ackageFlags: %d"), *PackageFileName, Package->GetPackageFlags());
				}
			}
			else
			{
				Package->ClearPackageFlags(PKG_DisallowExport);
			}
		});
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0
	UPackage::PackageSavedWithContextEvent.AddStatic([](const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext Context)
		{
			if (!FBSGuardCrypto::ShouldEncryptAsset(PackageFileName))
			{
				return;
			}
			if (!BSGEncrypt::IsPackageEncrypted(Package))
			{
				return;
			}
		
			// 当一个资产包保存成功后触发
			if (FBSGuardCrypto::IsEncryptedAssetFile(PackageFileName) == false)
			{
				Package->SetPackageFlags(PKG_DisallowExport);
				// 如果存在有效密钥，且当前保存的是未加密文件，将其加密
				if (FBSGuardCrypto::EncryptFile(PackageFileName))
				{
					UE_LOG(LogTemp, Log, TEXT("Encrypted asset on save: %s, ackageFlags: %d"), *PackageFileName, Package->GetPackageFlags());
				}
			}
			else
			{
				Package->ClearPackageFlags(PKG_DisallowExport);
			}
		});
#endif
	
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
		const FString& PackageExt = FBSGuardEditorModule::ChooseHeaderExt(AssetData);
		AssetFilePath = FPackageName::LongPackageNameToFilename(AssetFilePath, PackageExt);
		if (!FBSGuardCrypto::ShouldEncryptAsset(AssetFilePath))
		{
			continue;
		}
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
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 27
		FSlateIcon UnlockIcon(FEditorStyle::GetStyleSetName(), "Icons.Unlock");
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0 
		FSlateIcon UnlockIcon(FAppStyle::GetAppStyleSetName(), "Icons.Unlock");
#endif
		
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DecryptAssetLabel", "解密资产"),
			LOCTEXT("DecryptAssetTooltip", "使用当前密钥解密此资产文件"),
			UnlockIcon,  // 假设使用一个解锁图标
			FUIAction(
				FExecuteAction::CreateLambda([NeedDecryptFiles]()
				{
					for (const FAssetData& AssetData : NeedDecryptFiles)
					{
						UPackage* Package = AssetData.GetPackage();
						Package->ClearPackageFlags(PKG_DisallowExport);
						BSGEncrypt::MarkPackagePlain(Package);
						const FString& PackageExt = FBSGuardEditorModule::ChooseHeaderExt(AssetData);
						const FString FileName =FPackageName::LongPackageNameToFilename(Package->GetName(), PackageExt);
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
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 27
		FSlateIcon lockIcon(FEditorStyle::GetStyleSetName(), "Icons.Lock");
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0 
		FSlateIcon lockIcon(FAppStyle::GetAppStyleSetName(), "Icons.Lock");
#endif
		
		MenuBuilder.AddMenuEntry(
			LOCTEXT("EncryptAssetLabel", "加密资产"),
			LOCTEXT("EncryptAssetTooltip", "使用当前密钥加密此资产文件"),
			lockIcon,  // 假设使用一个锁图标
			FUIAction(
				FExecuteAction::CreateLambda([NeedEncryptionFiles]()
				{
					for (const FAssetData& AssetData : NeedEncryptionFiles)
					{
						UPackage* Package = AssetData.GetPackage();
						Package->SetPackageFlags(PKG_DisallowExport);
						BSGEncrypt::MarkPackageEncrypted(Package);
						const FString& PackageExt = FBSGuardEditorModule::ChooseHeaderExt(AssetData);
						const FString FileName =FPackageName::LongPackageNameToFilename(Package->GetName(), PackageExt);
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
	const FString& PackageExt = FBSGuardEditorModule::ChooseHeaderExt(AssetData);
	FString AssetFilePath = FPackageName::LongPackageNameToFilename(PackageName, PackageExt);

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
	const FString& PackageExt = FBSGuardEditorModule::ChooseHeaderExt(AssetData);
	FString AssetFilePath = FPackageName::LongPackageNameToFilename(PackageName, PackageExt);

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

bool FBSGE_AssetActions::CanMigrateEncryptedAssets()
{
	return false;
}

void FBSGE_AssetActions::CreateEncryptedAssetsMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
	MenuBuilder.AddMenuEntry(
			NSLOCTEXT("NTAssetGuard", "MigrateDisabled", "Migrate (加密资产禁用)"),
			NSLOCTEXT("NTAssetGuard", "MigrateDisabled_TT", "加密资产不可迁移"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction(), 
				FCanExecuteAction::CreateStatic(&FBSGE_AssetActions::CanMigrateEncryptedAssets)
			)
		);
}

TSharedRef<FExtender> FBSGE_AssetActions::MakeMigrateEncryptedAssets(const TArray<FAssetData>& SelectedAssets)
{
	auto Ext = MakeShared<FExtender>();
	bool state = false;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		FString PackageName = SelectedAssets[0].PackageName.ToString();
		const FString& PackageExt = FBSGuardEditorModule::ChooseHeaderExt(AssetData);
		FString AssetFilePath = FPackageName::LongPackageNameToFilename(PackageName, PackageExt);
		if (FBSGuardCrypto::IsEncryptedAssetFile(AssetFilePath))
		{
			state = true;
			break;
		}
	}
	
	if (state)
	{
		Ext->AddMenuExtension(
			"AssetContextAdvancedActions", 
			EExtensionHook::Before, 
			nullptr, 
			FMenuExtensionDelegate::CreateStatic(&FBSGE_AssetActions::CreateEncryptedAssetsMenu, SelectedAssets)
		);
	}
	return Ext;
}


FDelegateHandle FBSGE_AssetActions::ExtenderHandle;
//***********************************

const FString& FBSGuardEditorModule::ChooseHeaderExt(const FAssetData& AD)
{
	// ① 是否是关卡（UWorld）？
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 27
	const bool bIsWorld = (AD.AssetClass == UWorld::StaticClass()->GetFName());
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0
	const bool bIsWorld = (AD.AssetClassPath == UWorld::StaticClass()->GetClassPathName());
#endif
	// ② 项目当前是否以文本格式保存包？（你也可以改成读 Editor 设置）
	const bool bTextFormat = false;

	if (bIsWorld)
	{
		return bTextFormat
			? FPackageName::GetTextMapPackageExtension()   // ".utmap" :contentReference[oaicite:0]{index=0}
			: FPackageName::GetMapPackageExtension();      // ".umap"  :contentReference[oaicite:1]{index=1}
	}
	else
	{
		return bTextFormat
			? FPackageName::GetTextAssetPackageExtension() // ".utasset" :contentReference[oaicite:2]{index=2}
			: FPackageName::GetAssetPackageExtension();    // ".uasset"  :contentReference[oaicite:3]{index=3}
	}
}

void FBSGuardEditorModule::StartupModule()
{
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