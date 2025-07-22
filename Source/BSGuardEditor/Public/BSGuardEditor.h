// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FBSGuardEditorModule : public IModuleInterface
{
public:

	static const FString& ChooseHeaderExt(const FAssetData& AD); 
	
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
};
