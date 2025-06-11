// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FBSGuardCoreModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;


	static IPlatformFile* Base;
	static IPlatformFile* OriginalPlatformFile;
	static TUniquePtr<class FBSGuardPlatformFile> GuardPlatformFile;
private:
	TSharedPtr<class FBSGuardSettings> BSGuardSettings;
};
