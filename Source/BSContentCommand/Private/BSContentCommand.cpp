// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSContentCommand.h"

#include "ContentBrowserModule.h"
#include "SAssetView.h"
#include "StatusBarSubsystem.h"
#include "Framework/Commands/GenericCommands.h"
#include "Interfaces/IMainFrameModule.h"

#define LOCTEXT_NAMESPACE "FBSContentCommandModule"
static FDelegateHandle OnCreatedHandle;
void FBSContentCommandModule::StartupModule()
{
	// “抢占” Copy 命令，覆盖Ctrl+C 
	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	if (MainFrameModule.GetParentWindow().IsValid())
	{
		OnInitialize(MainFrameModule.GetParentWindow(), false);
	}
	else
	{
		MainFrameDelegateHandle = MainFrameModule.OnMainFrameCreationFinished().AddRaw(this, &FBSContentCommandModule::OnInitialize);
	}
	
}

void FBSContentCommandModule::ShutdownModule()
{
	// 移除命令扩展并解绑委托
	/*if (FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		FContentBrowserModule& CBModule = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
		CBModule.GetAllContentBrowserCommandExtenders().Remove(CommandExtenderHandle);
	}*/

	/*if (IMainFrameModule* MainFrameModule = FModuleManager::GetModulePtr<IMainFrameModule>("MainFrame"))
	{
		if (MainFrameDelegateHandle.IsValid())
		{
			MainFrameModule->OnMainFrameCreationFinished().Remove(MainFrameDelegateHandle);
			MainFrameDelegateHandle.Reset();
		}
	}*/
}

void FBSContentCommandModule::ExtendCBCommands(TSharedRef<FUICommandList> CommandList, FOnContentBrowserGetSelection GetSelectionDelegate) const
{
	// ① 想覆盖的命令（例如 Ctrl+C）
	const TSharedPtr<const FUICommandInfo> CopyCmd = FGenericCommands::Get().Copy;

	/* ---------- 把旧 Action 取出来 ---------- */
	TSharedPtr<FUIAction> OldActionPtr = MakeShared<FUIAction>(*CommandList->GetActionForCommand(CopyCmd));
	FUIAction OldAction = OldActionPtr.IsValid() ? *OldActionPtr : FUIAction();
	ECheckBoxState OldGetCheckState = CommandList->GetCheckState(CopyCmd.ToSharedRef());
	CommandList->UnmapAction(CopyCmd);

	/* ---------- 组装新的 FUIAction（包一层判断） ---------- */
	FExecuteAction NewExecute = FExecuteAction::CreateLambda([OldAction, GetSelectionDelegate]()
	{
		// 实时获取当前选区
		TArray<FAssetData> Assets;  TArray<FString> Paths;
		GetSelectionDelegate.ExecuteIfBound(Assets, Paths);

		// 自定义过滤：阻止含 Tag = NoCopy
		for (const FAssetData& AD : Assets)
		{
			FString Flag;
			if (AD.GetTagValue("NoCopy", Flag) && Flag == TEXT("True"))
			{
				FMessageDialog::Open(EAppMsgType::Ok,
					NSLOCTEXT("CopyGuard", "Blocked", "该资产禁止复制！"));
				return;                     // 拦截
			}
		}

		// 调旧的 Execute（若存在）
		if (OldAction.ExecuteAction.IsBound())
		{
			OldAction.ExecuteAction.Execute();
		}
	});

	FCanExecuteAction NewCanExecute = FCanExecuteAction::CreateLambda([OldAction, GetSelectionDelegate]()
	{
		// 先问旧的 CanExecute（若有）
		bool bOldOK =
			!OldAction.CanExecuteAction.IsBound() || OldAction.CanExecuteAction.Execute();

		if (!bOldOK)
			return false;

		// 也可在这里加额外条件；示例：必须至少选 1 项
		TArray<FAssetData> A; TArray<FString> P;
		GetSelectionDelegate.ExecuteIfBound(A, P);
		return A.Num() > 0;
	});

	/* ---------- 把新的 Action 重新绑定回同一命令 ---------- */
	CommandList->MapAction(CopyCmd, NewExecute);
}

void FBSContentCommandModule::ExecuteGuardedCopy(const TArray<FContentBrowserItem>& SelectedItems) const
{
	
}

bool FBSContentCommandModule::CanExecuteGuardedCopy(const TArray<FContentBrowserItem>& SelectedItems) const
{
	return true;
}

void FBSContentCommandModule::OnInitialize(TSharedPtr<SWindow> InRootWindow, bool bIsRunningStartupDialog)
{
	FContentBrowserModule& CBModule =  FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	
	CBModule.GetAllContentBrowserCommandExtenders().Add(CommandExtenderHandle);
	
	IContentBrowserSingleton& CBSingleton = CBModule.Get();

	//UStatusBarSubsystem* StatusBarSubsystem = GEditor->GetEditorSubsystem<UStatusBarSubsystem>();

	/*OnCreatedHandle = CBSingleton.GetOnContentBrowserCreated().AddLambda(
		[](const TSharedRef<SContentBrowser>& NewCB, const FString& /*Name#1#)
		{
			RemapCopyCommand(NewCB,
				FOnContentBrowserGetSelection::CreateSP(
					NewCB, &SContentBrowser::GetSelectedAssetsAndPaths));
		});*/
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBSContentCommandModule, BSContentCommand)