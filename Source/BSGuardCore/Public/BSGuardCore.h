//=============================================================
// Filename:       BSGuardEditor.h
// Publisher:      BigStar
// Creation Date:  2025-09-08
// Last Modified:  2025-09-08
// Version:        v1.0
//
// Description:
// BSGuardCoreModule.H
//=============================================================

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
