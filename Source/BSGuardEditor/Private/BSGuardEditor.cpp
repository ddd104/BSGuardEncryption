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

				if (!Tools.GetAssetTypeActionsForClass(TargetClass).IsValid())
				{
					return false;
				}
				TSharedPtr<IAssetTypeActions> Active = Tools.GetAssetTypeActionsForClass(TargetClass).Pin();

				if (Active == AssetTypeAction)
				{
					return true;
				}
			}
			return false;
		}
	);
	
	// Register for the save package event to automatically encrypt assets after they are saved
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
		
			// Triggered when an asset pack is saved successfully
			if (FBSGuardCrypto::IsEncryptedAssetFile(PackageFileName) == false)
			{
				Package->SetPackageFlags(PKG_DisallowExport);
				// If a valid key exists and the currently saved file is unencrypted, encrypt it
				if (FBSGuardCrypto::EncryptFile(PackageFileName))
				{
					UE_LOG(LogTemp, Log, TEXT("Encrypted asset on save: %s"), *PackageFileName);
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
		
			// Triggered when an asset pack is saved successfully
			if (FBSGuardCrypto::IsEncryptedAssetFile(PackageFileName) == false)
			{
				Package->SetPackageFlags(PKG_DisallowExport);
				// If a valid key exists and the currently saved file is unencrypted, encrypt it
				if (FBSGuardCrypto::EncryptFile(PackageFileName))
				{
					UE_LOG(LogTemp, Log, TEXT("Encrypted asset on save: %s"), *PackageFileName);
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
			"CommonAssetActions",
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
		// Check if the asset file is encrypted
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
		// **Decrypt** menu item
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 27
		FSlateIcon UnlockIcon(FEditorStyle::GetStyleSetName(), "Icons.Unlock");
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0 
		FSlateIcon UnlockIcon(FAppStyle::GetAppStyleSetName(), "Icons.Unlock");
#endif
		
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DecryptAssetLabel", "Decrypted Assets"),
			LOCTEXT("DecryptAssetTooltip", "Decrypt this asset file using the current key"),
			UnlockIcon,
			FUIAction(
				FExecuteAction::CreateLambda([NeedDecryptFiles]()
				{
					FBSGuardPlatformFile::Asset.Empty();
					for (const FAssetData& AssetData : NeedDecryptFiles)
					{
						UE_LOG(LogTemp, Display, TEXT("[%d] [EncryptSelectedAsset] Start DecryptSelectedAsset %s"), __LINE__, *AssetData.PackageName.ToString());
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
		// **Encrypt** menu item
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 27
		FSlateIcon lockIcon(FEditorStyle::GetStyleSetName(), "Icons.Lock");
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0 
		FSlateIcon lockIcon(FAppStyle::GetAppStyleSetName(), "Icons.Lock");
#endif
		
		MenuBuilder.AddMenuEntry(
			LOCTEXT("EncryptAssetLabel", "Encrypt Assets"),
			LOCTEXT("EncryptAssetTooltip", "Encrypt this asset file using the current key"),
			lockIcon,
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
		
		ResetLoaders(Package);
		TArray<UPackage*> ToUnload{ Package };
		UPackageTools::UnloadPackages(ToUnload);
		CollectGarbage(RF_NoFlags);
	}
	
	if (!FBSGuardCrypto::EncryptFile(AssetFilePath))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("EncryptFailed", "Failed to encrypt asset, please check logs."));
		return;
	}
	
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	ARM.Get().ScanFilesSynchronous({ AssetFilePath }, /*bForceRescan*/true);
	ARM.Get().WaitForCompletion();
	
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
		
		ResetLoaders(Package);
		TArray<UPackage*> ToUnload{ Package };
		UPackageTools::UnloadPackages(ToUnload);
		CollectGarbage(RF_NoFlags);
	}
	
	if (!FBSGuardCrypto::DecryptFile(AssetFilePath))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DecryptFailed", "加密资产失败，请检查日志。"));
		return;
	}

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	ARM.Get().ScanFilesSynchronous({ AssetFilePath }, /*bForceRescan*/true);
	ARM.Get().WaitForCompletion();
	
	UE_LOG(LogTemp, Display, TEXT("Asset %s Decrypted."), *PackageName);
}


FDelegateHandle FBSGE_AssetActions::ExtenderHandle;
//***********************************

void FBSGuardEditorModule::StartupModule()
{
	// Registering the Content Browser Menu Extension
	FBSGE_AssetActions::RegisterAssetActions();
	
	// Registering Encrypted AssetTypeAction
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