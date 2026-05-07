/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/
#pragma once

#include "IAssetTypeActions.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(InventorySXEditor, All, All);

class FInventorySXEditor : public IModuleInterface
{
public:
	
	static uint32 InventoryCategory;
	
	/* Called when the module is loaded */
	virtual void StartupModule() override;

	/* Called when the module is unloaded */
	virtual void ShutdownModule() override;

protected:
	TArray<TSharedPtr<IAssetTypeActions>> CreatedAssetTypeActions;
};
