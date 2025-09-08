//=============================================================
// Filename:       BSGuardEditor.h
// Publisher:      BigStar
// Creation Date:  2025-09-08
// Last Modified:  2025-09-08
// Version:        v1.0
//
// Description:
// Implement asset operations in the editor
//=============================================================

#pragma once

#include "Modules/ModuleManager.h"

class FBSGuardEditorModule : public IModuleInterface
{
public:
	
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
};
