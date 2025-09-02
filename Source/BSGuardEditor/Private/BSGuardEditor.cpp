// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSGuardEditor.h"

#include "AssetToolsModule.h"
#include "AssetTypeActions_EncryptedAsset.h"
#include "ContentBrowserModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "BSGuardCrypto.h"
#include "BSGuardPlatformFile.h"
#include "ContentBrowserDelegates.h"
#include "PackageTools.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/MessageDialog.h"
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

private:
	static FDelegateHandle ExtenderHandle;
	static TArray<TSharedPtr<FAssetTypeActions_EncryptedAsset>> GuardAssetActions;
};

TArray<TSharedPtr<FAssetTypeActions_EncryptedAsset>> FBSGE_AssetActions::GuardAssetActions;

void ScanEncryptedAssets()
{
	TArray<FString> FoundFiles;
	const FString ContentDir = FPaths::ProjectContentDir();
	IFileManager::Get().FindFilesRecursive(FoundFiles, *ContentDir, TEXT("*.uasset"), true, false);

	TArray<FString> EncryptedFiles;
	for (const FString& File : FoundFiles)
	{
		if (FBSGuardCrypto::IsEncryptedAssetFile(File))
		{
			EncryptedFiles.Add(File);
		}
	}

	if (EncryptedFiles.Num() > 0)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().ScanFilesSynchronous(EncryptedFiles);
	}
}

void FBSGE_AssetActions::RegisterAssetActions()
{
	bool ValidKey = FBSGuardCrypto::HasValidKey();
	if (ValidKey)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
		CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FBSGE_AssetActions::OnExtendContentBrowserAssetMenu));
		ExtenderHandle = CBMenuExtenderDelegates.Last().GetHandle();
		
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
		const FString& PackageExt = FBSGuardCrypto::ChooseHeaderExt(AssetData);
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
		FBSGuardPlatformFile::Asset.Empty();
		FBSGuardPlatformFile::Asset.Append(NeedDecryptFiles);
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
						UE_LOG(LogTemp, Display, TEXT("[FBSGE_AssetActions] [EncryptSelectedAsset] Start DecryptSelectedAsset %s"), *AssetData.PackageName.ToString());
						UPackage* Package = AssetData.GetPackage();
						Package->ClearPackageFlags(PKG_DisallowExport);
						BSGEncrypt::MarkPackagePlain(Package);
						const FString& PackageExt = FBSGuardCrypto::ChooseHeaderExt(AssetData);
						const FString FileName =FPackageName::LongPackageNameToFilename(Package->GetName(), PackageExt);
						UPackage::SavePackage(Package, nullptr, RF_Standalone, *FileName, nullptr, nullptr, false, true, SAVE_NoError);
						bool Ret = Package->MarkPackageDirty();
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
						const FString& PackageExt = FBSGuardCrypto::ChooseHeaderExt(AssetData);
						const FString FileName =FPackageName::LongPackageNameToFilename(Package->GetName(), PackageExt);
						UPackage::SavePackage(Package, nullptr, RF_Standalone, *FileName, nullptr, nullptr, false, true, SAVE_NoError);
						bool Ret = Package->MarkPackageDirty();
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
	const FString PackageName = AssetData.PackageName.ToString();
	const FString& PackageExt = FBSGuardCrypto::ChooseHeaderExt(AssetData);
	const FString AssetFilePath = FPackageName::LongPackageNameToFilename(PackageName, PackageExt);

	UPackage* Package = AssetData.GetPackage();
	if (Package)
	{
		Package->FullyLoad();

		// 1) 先保存最新内容（你已有）
		bool bOk = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
		FSavePackageArgs Args;
		bOk = UPackage::SavePackage(Package, AssetData.GetAsset(), *AssetFilePath, Args);
#else
		bOk = UPackage::SavePackage(Package, nullptr, RF_Public|RF_Standalone, *AssetFilePath, nullptr, nullptr, false, true, SAVE_NoError);
#endif
		if (!bOk)
		{
			UE_LOG(LogTemp, Error, TEXT("SaveFile failed for %s"), *PackageName);
			return;
		}

		// 2) 释放引用（避免后续文件被 UE 持有）
		ResetLoaders(Package);
		TArray<UPackage*> ToUnload{ Package };
		UPackageTools::UnloadPackages(ToUnload);
		CollectGarbage(RF_NoFlags);
	}

	// 3) 物理层加密
	if (!FBSGuardCrypto::EncryptFile(AssetFilePath))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("EncryptFailed", "加密资产失败，请检查日志。"));
		return;
	}

	// 4) 通知资产注册表重扫该文件（刷新缩略图/状态）
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	ARM.Get().ScanFilesSynchronous({ AssetFilePath }, /*bForceRescan*/true);
	ARM.Get().WaitForCompletion();

	// 5) 不要 MarkPackageDirty；如需重开编辑器，可按需打开
	UE_LOG(LogTemp, Display, TEXT("Asset %s Encrypted."), *PackageName);
}

void FBSGE_AssetActions::DecryptSelectedAsset(const FAssetData& AssetData)
{
	const FString PackageName = AssetData.PackageName.ToString();
	const FString& PackageExt = FBSGuardCrypto::ChooseHeaderExt(AssetData);
	const FString AssetFilePath = FPackageName::LongPackageNameToFilename(PackageName, PackageExt);

	UPackage* Package = AssetData.GetPackage();
	if (Package)
	{
		Package->FullyLoad();

		// 1) 先保存最新内容（你已有）
		bool bOk = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
		FSavePackageArgs Args;
		bOk = UPackage::SavePackage(Package, AssetData.GetAsset(), *AssetFilePath, Args);
#else
		bOk = UPackage::SavePackage(Package, nullptr, RF_Public|RF_Standalone, *AssetFilePath, nullptr, nullptr, false, true, SAVE_NoError);
#endif
		if (!bOk)
		{
			UE_LOG(LogTemp, Error, TEXT("SaveFile failed for %s"), *PackageName);
			return;
		}

		// 2) 释放引用（避免后续文件被 UE 持有）
		ResetLoaders(Package);
		TArray<UPackage*> ToUnload{ Package };
		UPackageTools::UnloadPackages(ToUnload);
		CollectGarbage(RF_NoFlags);
	}

	// 3) 物理层加密
	if (!FBSGuardCrypto::DecryptFile(AssetFilePath))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DecryptFailed", "加密资产失败，请检查日志。"));
		return;
	}

	// 4) 通知资产注册表重扫该文件（刷新缩略图/状态）
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	ARM.Get().ScanFilesSynchronous({ AssetFilePath }, /*bForceRescan*/true);
	ARM.Get().WaitForCompletion();

	// 5) 不要 MarkPackageDirty；如需重开编辑器，可按需打开
	UE_LOG(LogTemp, Display, TEXT("Asset %s Decrypted."), *PackageName);
}


FDelegateHandle FBSGE_AssetActions::ExtenderHandle;
//***********************************

void FBSGuardEditorModule::StartupModule()
{
	// 注册内容浏览器菜单扩展
	FBSGE_AssetActions::RegisterAssetActions();
	
	// 注册加密AssetTypeAction
	FBSGE_AssetActions::RegisterAssetTypeAction();

	ScanEncryptedAssets();
	
}

void FBSGuardEditorModule::ShutdownModule()
{
	FBSGE_AssetActions::UnregisterAssetActions();
	FBSGE_AssetActions::UnregisterAssetTypeAction();
}




#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBSGuardEditorModule, BSGuardEditor)