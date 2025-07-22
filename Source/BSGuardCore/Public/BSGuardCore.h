// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/IPlatformFileModule.h"
#include "Modules/ModuleManager.h"

class FBSGuardCoreModule : public IPlatformFileModule
{
public:
	virtual IPlatformFile* GetPlatformFile() override;
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;


	

};
