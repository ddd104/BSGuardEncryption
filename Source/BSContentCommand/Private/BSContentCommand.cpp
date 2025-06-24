// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSContentCommand.h"

#include "ContentBrowserModule.h"
#include "Framework/Commands/GenericCommands.h"

#define LOCTEXT_NAMESPACE "FBSContentCommandModule"

void FBSContentCommandModule::StartupModule()
{
	

	
	// “抢占” Copy 命令，覆盖Ctrl+C 
	{
		FContentBrowserModule& CBModule =  FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		CommandExtenderHandle = FContentBrowserCommandExtender::CreateRaw(this, &FBSContentCommandModule::ExtendCBCommands);

		CBModule.GetAllContentBrowserCommandExtenders().Add(CommandExtenderHandle);
	}
}

void FBSContentCommandModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

void FBSContentCommandModule::ExtendCBCommands(TSharedRef<FUICommandList> CommandList, FOnContentBrowserGetSelection GetSelectionDelegate) const
{
	CommandList->UnmapAction(FGenericCommands::Get().Copy);
	
	
	CommandList->MapAction(
		FGenericCommands::Get().Copy,
		/*-------------- Execute --------------*/
		FExecuteAction::CreateLambda([GetSelectionDelegate]()
		{
			/*// a. 获取当前选区
			TArray<FAssetData> Assets;
			TArray<FString>   Paths;
			GetSelectionDelegate.ExecuteIfBound(Assets, Paths);

			// b. 自己的过滤逻辑：示例—阻止带元数据 "NoCopy" == "True"
			for (const FAssetData& AD : Assets)
			{
				FString Flag;
				if (AD.GetTagValue("NoCopy", Flag) && Flag == TEXT("True"))
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						NSLOCTEXT("CopyGuard", "CopyBlocked", "该资产禁止复制！"));
					return;                                     // 拦截
				}
			}

			// c. 允许 → 调用官方实现
			ContentBrowserUtils::CopyItemObjectPathText(Assets);*/
			UE_LOG(LogTemp, Log, TEXT("Copy"));
		}),

		/*----------- CanExecute -------------*/
		FCanExecuteAction::CreateLambda([GetSelectionDelegate]()
		{
			/*TArray<FAssetData> TmpA; TArray<FString> TmpP;
			GetSelectionDelegate.ExecuteIfBound(TmpA, TmpP);
			return TmpA.Num() > 0;          // 仅有选中资产时允许*/
			return true;
		})
	);
}

void FBSContentCommandModule::ExecuteGuardedCopy(const TArray<FContentBrowserItem>& SelectedItems) const
{
	
}

bool FBSContentCommandModule::CanExecuteGuardedCopy(const TArray<FContentBrowserItem>& SelectedItems) const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBSContentCommandModule, BSContentCommand)