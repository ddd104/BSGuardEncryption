// Copyright (c) 2025 BigStar. All Rights Reserved.

#pragma once
#include "Misc/PackageName.h"
#include "Misc/MessageDialog.h"
#include "UObject/Package.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFile.h"
#include "UObject/UObjectGlobals.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"

class FBSGuardEditorModule : public IModuleInterface
{
public:
	
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
};
