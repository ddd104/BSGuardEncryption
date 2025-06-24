// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ContentBrowserDelegates.h"
#include "Modules/ModuleManager.h"

class FBSContentCommandModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	/** 把 Copy 命令换成我们自己的实现 */
	void ExtendCBCommands(TSharedRef<FUICommandList> CommandList, FOnContentBrowserGetSelection GetSelectionDelegate) const;
	/** 执行+过滤逻辑 */
	void ExecuteGuardedCopy(const TArray<FContentBrowserItem>& SelectedItems) const;
	bool CanExecuteGuardedCopy(const TArray<FContentBrowserItem>& SelectedItems) const;
	/** 保存返回的句柄，Shutdown 时要解绑 */
	FContentBrowserCommandExtender CommandExtenderHandle;
};
