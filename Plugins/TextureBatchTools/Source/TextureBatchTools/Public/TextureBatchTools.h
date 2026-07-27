#pragma once

#include "Modules/ModuleManager.h"
#include "Delegates/IDelegateInstance.h"

class FTextureBatchToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Handle to the Content Browser context-menu extender we registered. */
	FDelegateHandle ContentBrowserExtenderHandle;
};
