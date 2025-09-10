// Copyright (c) 2025 BigStar. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FBSGuardEditorModule : public IModuleInterface
{
public:
	
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
};
